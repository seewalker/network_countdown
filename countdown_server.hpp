#include <experimental/optional>
#include <numeric>
#ifndef COUNTDOWN_COMMON
    #include "countdown_common.hpp"
#endif
#include <algorithm>

const int BACKLOG = 10;

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
int init_socket(std::string server,int &sock,int port) {
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
    addr.sin_addr.s_addr = inet_addr(server.c_str());
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

std::string shutdown_msg( ) {
    std::ostringstream oss;
    oss << "shutdown" << std::endl;
    return validate_wrap(oss.str());
}

// this should take an optional string, since the owner of the lock may not be known.
std::string unlock_msg(std::experimental::optional<std::string> nickname) {
    if (nickname == std::experimental::nullopt) {
        return lock_common_msg("unlock","");
    }
    else {
        return lock_common_msg("unlock",*nickname);
    }
}

// to broacast to literally everyone, use a negative mask.
// to relay to everyone but original sender, use mask with relevant fd.
int broadcast(int mask,std::vector<struct client_meta> client_metas,const char *msgbuf, int msglen) {
    int n_fail = 0;
    int j;
    for(j=0;j<client_metas.size();++j) {
        if (j != mask) {
            // broadcast the goodbye message.
            if (write(client_metas[j].sock,msgbuf,msglen) < 0) {
                std::cerr << "Failed to write during broadcast on client " << client_metas[j].sock << std::endl;
                ++n_fail;
            }
        }
    }
    return n_fail;
}

std::vector<int> client_socks(std::vector<struct client_meta> metas) {
    std::vector<int> socks;
    for (auto m : metas) {
        socks.push_back(m.sock);
    }
    return socks;
}

namespace countdown_server {
    std::string parse_cmdline(std::string cmd) {
        std::string msg_t;
        std::istringstream iss(cmd);
        iss >> msg_t;
        if (msg_t == "q" || msg_t == "quit" || msg_t == "exit") {
            return shutdown_msg( );
        }
        else {
            throw std::invalid_argument("Not in the interactive command line language");
        }
    }
    void mk_fd_sets(int listen_socket,std::vector<int> client_sockets,fd_set *read_fds,fd_set *write_fds,fd_set *except_fds) {
        FD_ZERO(read_fds);
        FD_SET(listen_socket,read_fds);
        // lisen for interactive command line.
        FD_SET(STDIN_FILENO,read_fds);
        // listen for each connection.
        for (auto s : client_sockets) {
            FD_SET(s,read_fds);
        }
        // for now these will be unused.
        FD_ZERO(write_fds);
        FD_ZERO(except_fds);
    }
    int interact(std::string server,int port,int ping_n) {
        int listen_sock,hi_sock,ret,client_sock,n_countdowns=0,i,j,msglen,msglen_synch,seq,n;
        std::vector<client_meta> client_metas;
        std::experimental::optional<std::string> lock_owner = std::experimental::nullopt;
        std::string msg,t_sent,unlock_msgbuf,cmd;
        std::time_t t0,tf;
        char msgbuf[MAX_MSGLEN],msgbuf_synch[MAX_MSGLEN],msgwritebuf_synch[MAX_MSGLEN];
        fd_set read_fds,write_fds,except_fds;
        double t_delay;
        std::cout << "Starting with port = " << port << ", ping_n = " << ping_n << std::endl;
        init_socket(server,listen_sock,port);
        while (true) {
            auto cs = client_socks(client_metas);
            countdown_server::mk_fd_sets(listen_sock,cs,&read_fds,&write_fds,&except_fds);
            hi_sock = (client_metas.size() == 0) ? listen_sock : *std::max_element(cs.begin(),cs.end());
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
                        else { std::cerr << "Failed to accept incoming connection" << std::endl; }
                        // establish the number of pings.
                        msg = ping_init_msg(ping_n);
                        write(client_sock,msg.c_str(),msg.length());
                    }
                    // server can do a shutdown on stdin.
                    if (FD_ISSET(STDIN_FILENO,&read_fds)) {
                        try {
                            std::getline(std::cin,cmd);
                            auto msg = countdown_server::parse_cmdline(cmd);
                            broadcast(-1,client_metas,msg.c_str(),msg.length());
                            switch(classify(msg.c_str())) {
                                case SHUTDOWN:
                                    std::cout << "Shutting down." << std::endl;
                                    return 0;
                                default:
                                    break;
                            }
                        }
                        catch (const std::exception& e) {

                        }
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
                                        std::cout << "Got hello message from " << client_meta.nickname << std::endl;
                                        if (broadcast(i,client_metas,msgbuf,msglen) > 0) {
                                            std::cerr << "" << std::endl;
                                        }
                                    }
                                    catch (const std::exception& e) { std::cerr << "Could not extract nickname from hello message, so not passing it along." << std::endl; }
                                    break;
                                case GOODBYE:
                                    std::cout << "Got goodbye message from " << client_meta.nickname << std::endl;
                                    delete_idxs.push_back(i);
                                    if (broadcast(i,client_metas,msgbuf,msglen) > 0) {
                                        
                                    }
                                    break;
                                case PEER_LOCK:
                                    try {
                                        std::string lock_nickname,lock_date;
                                        lock_nickname = parse_lock_msg(msgbuf,lock_date);
                                        std::cout << "Got lock message from " << client_meta.nickname << std::endl;
                                        if (lock_owner != std::experimental::nullopt) { std::cerr << "Warning, multiple people typing command at once" << std::endl; }
                                        else { lock_owner = lock_nickname; }
                                        if (broadcast(i,client_metas,msgbuf,msglen) > 0) {
                                            
                                        }
                                    }
                                    catch (const std::exception& e) {
                                        std::cerr << "Could not parse lock message, so not passing it anywhere." << std::endl;
                                    }
                                    break;
                                case PEER_UNLOCK:
                                    try {
                                        std::string lock_nickname,lock_date;
                                        lock_nickname = parse_unlock_msg(msgbuf,lock_date);
                                        std::cout << "Got unlock message from " << client_meta.nickname << std::endl;
                                        if (lock_owner != lock_nickname) { std::cerr << "Warning, got unlock message from someone other than current lock owner." << std::endl; }
                                        else { lock_owner = std::experimental::nullopt; }
                                        if (broadcast(i,client_metas,msgbuf,msglen) > 0) {
                                           
                                        }
                                    }
                                    catch (const std::exception& e) {
                                        std::cerr << "Exception when recieving unlock message." << std::endl;
                                    }
                                    break;
                                case WRITE_ARBITRARY:
                                    try {
                                        auto write_arg = parse_write_msg(msgbuf);
                                        std::cout << "Got write message from " << client_meta.nickname << "with content '" << write_arg << "'" << std::endl;

                                        if (broadcast(i,client_metas,msgbuf,msglen) > 0) {
                                            std::cerr << "Failed to send write message to some recipients" << std::endl;
                                        }
                                        // send unlock.
                                        unlock_msgbuf = unlock_msg(lock_owner);
                                        if (broadcast(-1,client_metas,unlock_msgbuf.c_str(),unlock_msgbuf.length()) > 0) {
                                            std::cerr << "Failed to send unlock message to some recipients" << std::endl;
                                        }
                                    }
                                    catch (std::exception e) {
                                        std::cerr << "Recieved invalid write message, having exceptionL " << e.what() << std::endl;
                                    }
                                    break;
                                case COUNTDOWN_N:
                                    // do the synchronous ping loops.
                                    try {
                                        std::string countdown_msg(msgbuf);
                                        n = parse_countdown_n_msg(countdown_msg);
                                        std::cout << "Got countdown message from " << client_meta.nickname << std::endl;
                                    }
                                    catch (std::exception e) {
                                        std::cerr << e.what() << std::endl << "So exiting the countdown." << std::endl;
                                        break;
                                    }
                                    for(i=0;i<ping_n;++i) {
                                        // store this once as a c-string ready to write.
                                        strcpy(msgwritebuf_synch,ping_msg(i).c_str());
                                        msglen_synch = strlen(msgwritebuf_synch);
                                        for(j=0;j<client_metas.size();++j) {
                                            t0 = now();
                                            // this line is misbehaving somehow, i think.
                                            write(client_metas[j].sock,msgwritebuf_synch,msglen_synch);
                                            try {
                                                // this got a unlock message.
                                                recvloop(client_metas[j].sock,msgbuf_synch); // will throw if needs to.
                                                tf = now();
                                                seq = parse_ping_msg(msgbuf_synch);
                                                if (seq == i) { client_metas[j].rtts.push_back(tf-t0); }
                                                else { std::cerr << "Recieved out of order sequence number from client" << std::endl; }
                                            }
                                            catch (const std::exception& e ) { std::cerr << "ping loop error: " << e.what() << std::endl; }
                                        }
                                    }
                                    // sending countdown orders is not as simple as a broadcast because each must have its own delay and custom message.
                                    for(j=0;j<client_metas.size();++j) {
                                        t_delay = client_metas[j].estimate_send_t();
                                        auto ordermsg = countdown_order_msg(n,t_delay);
                                        write(client_metas[j].sock,ordermsg.c_str(),ordermsg.length());
                                    }
                                    // send unlock message.
                                    unlock_msgbuf = unlock_msg(lock_owner);
                                    if (broadcast(-1,client_metas,unlock_msgbuf.c_str(),unlock_msgbuf.length()) > 0) {
                                        std::cout << "Failed to send unlock message" << std::endl;
                                    }
                                    ++n_countdowns;
                                    break;
                                case PING:
                                    std::cerr << "A ping message has been recieved asynchronously, which should not happen." << std::endl;
                                    break;
                                case COUNTDOWN_INTERRUPT:
                                case COUNTDOWN_ORDER:
                                case PING_INIT:
                                case SHUTDOWN:
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
}

// defining this as a function rather than as just text in the main program so that it can be executed as a test as well.
