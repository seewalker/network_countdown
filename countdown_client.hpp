#ifndef COUNTDOWN_COMMON
    #include "countdown_common.hpp"
#endif
std::string format_say_countdown(int n,double delay) {
    std::ostringstream oss;
    oss << "sleep " << delay << " && ";
    for(;n>0;--n) {
        oss << "say " << n << " && sleep 1 && ";
    }
    oss << "say play";
    return oss.str();
}

std::string format_say_raw(std::string saying) {
    std::ostringstream oss;
    oss << "say ";
    oss << saying;
    return oss.str();
}


std::string lock_msg(std::string nickname) {
    return lock_common_msg("lock",nickname);
}


std::string countdown_n_msg(int start_idx) {
    std::ostringstream oss;
    oss << "countdown_n " << start_idx << std::endl;
    return validate_wrap(oss.str());
}

int parse_ping_init_msg(std::string cmd) {
    int n=-1;
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != "init_ping") { throw std::domain_error("Message type is not what is expected."); }
    iss >> n;
    if (n < 1) { throw std::domain_error("Invalid value for ping n"); }
    return n;
}

// a countdown order comes from the server and has a rtt-based delay with it.
void parse_countdown_order_msg(std::string cmd,int &start_idx,double &delay) {
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != "countdown_order") { throw std::domain_error("Message type is not what is expected"); }
    iss >> start_idx;
    iss >> delay;
}

// this is parsing what is coming in over the command line.
// buf is a whole line of input.
// returns a message which can go over the network.
std::string parse_cmdline(std::string buf,std::string nickname) {
    int countdown_n = DEFAULT_COUNTDOWN_N;
    std::string msg_t,remainder;
    std::istringstream iss(buf);
    iss >> msg_t;
    // process functions without arguments
    if (msg_t == "q" || msg_t == "quit" || msg_t == "exit") {
        return goodbye_msg(nickname);
    }
    else if (msg_t == "countdown") {
        // if there is no more input to read, then countdown_n goes unchanged from the default.
        iss >> countdown_n;
        return countdown_n_msg(countdown_n);
    }
    else if (msg_t == "write" || msg_t == "w") {
        std::getline(iss,remainder);
        return write_msg(remainder);
    }
    else { throw std::invalid_argument("Not in the interactive command line language"); }
}

void mk_client_fd_sets(int srv_socket,fd_set *read_fds,fd_set *write_fds,fd_set *except_fds) {
    FD_ZERO(read_fds);
    FD_SET(STDIN_FILENO,read_fds);
    FD_SET(srv_socket,read_fds);
    // for now these will be unused.
    FD_ZERO(write_fds);
    FD_ZERO(except_fds);
}
