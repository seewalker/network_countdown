/* Notes
 * There is a socket which is for listening for new clients. This socket gets "bind" and "listen" called on it. Listen takes the socket (which is a file descriptor) and also a backlog parameter for rate-limiting new connections (somehow, the details are gory?).
 * The per-client sockets are the return value of accept.
 *
 * For now all writes can happen outside the select. Later I can move them in as I see what the point is.
 *
 * In the server, writes should be carefully timed. Make expected time to be recieved roughly the same (maybe even by crafting an initial sleep time).
 *  There should be measurement here of just how synchronized it was.
 *
 *  setsockopt has a "level" argument which can be used to set options within TCP or at the higher socket level. that's where the constant SOL_SOCKET plays a role.
 */

#include <experimental/optional>
#include <numeric>
#include "countdown_common.cpp"

#define EXTERNAL_IPV4 "45.33.122.23"

const int BACKLOG = 10;

/* Here's how pings work.
 *  n rounds of blocking writes and recvs on the current file descriptor.
 *  server seq t_sent_serverclock
 *      .(keep a clock going). when i get a message back, that is a round trip.
 *  client seq t_recv_clientclock t_sent_clientclock.
 *      this gives the halfway-between time on the client's clock.
 */
// maybe for now there can be the modeling assumption that one trip is half an rtt.
struct client_meta {
    int sock;
    struct sockaddr_in addr;
    std::string nickname;

    std::vector<double> rtts;
    std::vector<double> prev_rtt_estimates;
    double estimate_send_t( ) {
        // don't include the first rtt because that one happened async and may be an overestimate.
        double mean_rtt = std::accumulate(rtts.begin() + 1,rtts.end(),0) / rtts.size();
        return mean_rtt / 2;
    }
};

// select will do just about everything we need here.
int init_socket(int &sock,int port) {
    int reuse = 1;
    struct sockaddr_in addr;

    if ((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        std::cerr << "Failed to make socket" << std::endl;
        return -1;
    }
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) != 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        return -1;
    }
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(EXTERNAL_IPV4);
    addr.sin_port = htons(port);
    if (bind(sock,(struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
        std::cerr << " " << std::endl;
        return -1;
    }
    if (listen(sock,BACKLOG) != 0) {
        std::cerr << " " << std::endl;
        return -1;
    }
    return 0;
}

char* repr_addr(struct sockaddr_in addr) {
    char *ipv4 = new char[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&addr.sin_addr,ipv4,INET_ADDRSTRLEN);
    return ipv4;
}

int process_connection(int listen_sock,struct sockaddr_in *addr) {
    char *client_ipv4;
    int sock;
	socklen_t client_len;
    client_len = sizeof(addr);
    memset(addr,0,client_len);
    if ((sock = accept(listen_sock,(struct sockaddr *)addr,&client_len)) < 0) {
        std::cerr << " " << std::endl;
        return -1;
    }
    client_ipv4 = repr_addr(*addr);
    std::cout << "New connection from " << client_ipv4 << std::endl;
    return sock;
}

void mk_server_fd_sets(int listen_socket,std::vector<int> client_sockets,fd_set *read_fds,fd_set *write_fds,fd_set *except_fds) {
    FD_ZERO(read_fds);
    FD_SET(listen_socket,read_fds);
    for (auto s : client_sockets) {
        FD_SET(s,read_fds);
    }
    // for now these will be unused.
    FD_ZERO(write_fds);
    FD_ZERO(except_fds);
}

// Messages only the server will send.
std::string ping_init_msg(int ping_n) {
    std::ostringstream oss;
    oss << "init_ping " << ping_n << std::endl;
    return validate_wrap(oss.str());
}

std::string countdown_order_msg(int start_idx,double delay) {
    std::ostringstream oss;
    oss << "countdown_order " << start_idx << " " << delay << std::endl;
    return validate_wrap(oss.str());
}

// to broacast to literally everyone, use a negative mask.
// to relay to everyone but original sender, use mask with relevant fd.
int broadcast(int mask,std::vector<struct client_meta> client_metas,char *msgbuf, int msglen) {
    int all_success = 1;
    int j;
    for(j=0;j<client_metas.size();++j) {
        if (j != mask) {
            // broadcast the goodbye message.
            if (write(client_metas[j].sock,msgbuf,msglen) < 0) {
                std::cerr << "Failed to write during broadcast on client " << client_metas[j].sock << std::endl;
                all_success = 0;
            }
        }
    }
    return all_success;
}

std::vector<int> client_socks(std::vector<struct client_meta> metas) {
    std::vector<int> socks;
    for (auto m : metas) {
        socks.push_back(m.sock);
    }
    return socks;
}

int main(int argc, char **argv) {
    int listen_sock,port,hi_sock,ret,client_sock,ping_n,n_countdowns=0,i,j,msglen,msglen_synch,seq;
    std::vector<client_meta> client_metas;
    std::experimental::optional<std::string> lock_owner = std::experimental::nullopt;
    std::string msg;
    std::time_t t0,tf;
    // the synchronous one is for sending and recieving messages synchronously within an asynch event.
    char msgbuf[MAX_MSGLEN],msgbuf_synch[MAX_MSGLEN];
    std::string t_sent;
    fd_set read_fds,write_fds,except_fds;
    cxxopts::Options options("Countdown Client","A program that will coordinate a countdown.");
    // Tne number of pings applies both to the before and after of a countdown (the first one, to get up-to-date estimates on latencies. the latter to see how close it was to a successful countdown.).
    options.add_options()
        ("p,port","Port",cxxopts::value<int>())
        ("n,ping_n","Number of pings",cxxopts::value<int>())
    ;
    auto result = options.parse(argc,argv);
    try {
        port = result["port"].as<int>();
        ping_n = result["ping_n"].as<int>();
    }
    catch(cxxopts::OptionException e) {
        std::cerr << "Option Error:" << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }
    init_socket(listen_sock,port);
    while (true) {
        auto cs = client_socks(client_metas);
        mk_server_fd_sets(listen_sock,cs,&read_fds,&write_fds,&except_fds);
        if (client_metas.size() == 0) {
            hi_sock = listen_sock;
        }
        else {
            auto argmax = std::max_element(cs.begin(),cs.end());
            hi_sock = *argmax;
        }
        ret = select(hi_sock + 1,&read_fds,&write_fds,&except_fds,NULL);
        switch (ret) {
            case -1:
                std::cerr << "Error in select" << std::endl;
                break;
            case 0:
                std::cerr << "Error in select" << std::endl;
                break;
            default:
                if (FD_ISSET(listen_sock,&read_fds)) {
                    struct client_meta meta;
                    if ((client_sock = process_connection(listen_sock,&meta.addr)) > 0) {
                        meta.sock = client_sock;
                        client_metas.push_back(meta);
                    }
                    else {
                        std::cerr << "Failed to accept incoming connection" << std::endl;
                    }
                    // establish the number of pings.
                    msg = ping_init_msg(ping_n);
                    write(client_sock,msg.c_str(),msg.length());
                }
                std::vector<int> delete_idxs;
                for (i=0;i<client_metas.size(); ++i) {
                    auto & client_meta = client_metas[i];
                    if (FD_ISSET(client_meta.sock,&read_fds)) {
                        // do the recv into recvbuf
                        msglen = recvloop(client_meta.sock,msgbuf);
                        switch (classify(msgbuf)) {
                            case HELLO:
                                try {
                                    client_meta.nickname = parse_hello(msgbuf);
                                    broadcast(i,client_metas,msgbuf,msglen);
                                }
                                catch (const std::exception& e) {
                                    std::cerr << "Could not extract nickname from hello message, so not passing it along." << std::endl;
                                    // is this a good place to close the connection?
                                }
                                break;
                            case GOODBYE:
                                delete_idxs.push_back(i);
                                broadcast(i,client_metas,msgbuf,msglen);
                                break;
                            case PEER_LOCK:
                                try {
                                    std::string lock_nickname,lock_date;
                                    lock_nickname = parse_lock_msg(msgbuf,lock_date);
                                    if (lock_owner != std::experimental::nullopt) {
                                        std::cerr << "Warning, multiple people typing command at once" << std::endl;
                                    }
                                    else {
                                        lock_owner = lock_nickname;
                                    }
                                    broadcast(i,client_metas,msgbuf,msglen);
                                }
                                catch (const std::exception& e) {
                                    std::cerr << "Could not parse lock message, so not passing it anywhere." << std::endl;
                                }
                                break;
                            case PEER_UNLOCK:
                                try {
                                    std::string lock_nickname,lock_date;
                                    lock_nickname = parse_unlock_msg(msgbuf,lock_date);
                                    if (lock_owner != lock_nickname) {
                                        std::cerr << "Warning, got unlock message from someone other than current lock owner." << std::endl;
                                    }
                                    else {
                                        lock_owner = std::experimental::nullopt;
                                    }
                                }
                                catch (const std::exception& e) {

                                }
                                broadcast(i,client_metas,msgbuf,msglen);
                                break;
                            case WRITE_ARBITARY:
                                broadcast(i,client_metas,msgbuf,msglen);
                                break;
                            case COUNTDOWN_N:
                                // do the synchronous ping loops.
                                for(i=0;i<ping_n;++i) {
                                    msg = ping_msg(i);
                                    for(j=0;j<client_metas.size();++j) {
                                        t0 = now();
                                        write(client_metas[i].sock,msg.c_str(),msg.length());
                                        // I should automate the recv loop for these messages.
                                        try {
                                            auto ping_ret = recvloop(client_metas[i].sock,msgbuf);
                                        }
                                        catch (const std::exception& e ) {

                                        }
                                        tf = now();
                                        try {
                                            seq = parse_ping_msg(msg);
                                            if (seq != i) {
                                                std::cerr << "Recieved out of order sequence number from client" << std::endl;
                                            }
                                            else {
                                                client_metas[i].rtts.push_back(tf-t0);
                                            }
                                        }
                                        catch (const std::exception& e ) {
                                            std::cerr << "Recieved malformed ping message" << std::endl;
                                        }
                                    }
                                }
                                // -1 means send even to the originating client.
                                broadcast(-1,client_metas,msgbuf,msglen);
                                ++n_countdowns;
                                break;
                            case PING:
                                std::cerr << "A ping message has been recieved asynchronously, which should not happen." << std::endl;
                                break;
                            case COUNTDOWN_INTERRUPT:
                            case COUNTDOWN_ORDER:
                            case PING_INIT:
                            case HELLO_ACK:
                                std::cerr << "The server should never recieve this message. It should only send it." << std::endl;
                                break;
                            case MALFORMED:
                                std::cerr << "The message was not even recognized on the first pass. Discarding it." << std::endl;
                                break;
                        }
                    }
                }
                // traverse the indexes backwards.
                std::reverse(delete_idxs.begin(),delete_idxs.end());
                for (auto i : delete_idxs) {
                    client_metas.erase(client_metas.begin() + i);
                }
        }
    }
}
