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
    addr.sin_addr.s_addr = inet_addr(DEFAULT_SERVER);
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
