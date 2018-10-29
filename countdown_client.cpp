// In the client, writes should happen ASAP.
#include "countdown_common.cpp"

namespace Color {
    enum Code {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };
    class Modifier {
        Code code;
    public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
        operator<<(std::ostream& os, const Modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };
}

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

int parse_countdown_n_msg(std::string cmd) {
    return parse_unary_int("countdown_n",cmd);
}

int parse_ping_init_msg(std::string cmd) {
    int n;
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != "init_ping") {
        throw std::domain_error(" ");
    }
    iss >> n;
    if (n < 1) {
        throw std::domain_error(" ");
    }
    return n;
}

// a countdown order comes from the server and has a rtt-based delay with it.
void parse_countdown_order_msg(std::string cmd,int &start_idx,double &delay) {
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != "countdown_order") {
        throw std::domain_error(" ");
    }
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
    else {
        throw std::invalid_argument("Not in the interactive command line language");
    }
}

void mk_client_fd_sets(int srv_socket,fd_set *read_fds,fd_set *write_fds,fd_set *except_fds) {
    FD_ZERO(read_fds);
    FD_SET(STDIN_FILENO,read_fds);
    FD_SET(srv_socket,read_fds);
    // for now these will be unused.
    FD_ZERO(write_fds);
    FD_ZERO(except_fds);
}

int main (int argc, char **argv) {
    // parse command line arguments.
    std::string nickname,server,cmd,write_buffer,srv_buffer,msg,t_sent,peer_name,cmdline;
    int port,srv_socket,ping_n=-1,countdown_start,i,seq,msglen_synch;
    double countdown_delay;
    bool chatty;
    std::vector<std::string> peer_names;
    struct sockaddr_in srv_addr;
    fd_set read_fds,write_fds,except_fds;
    char srv_readbuf[MAX_MSGLEN],msgbuf_synch[MAX_MSGLEN];

    cxxopts::Options options("Countdown Client","A program that will coordinate a countdown.");
    // Parse Command Line Args
    options.add_options()
        ("n,nickname","Nickname",cxxopts::value<std::string>())
        ("p,port","Port",cxxopts::value<int>()->default_value(DEFAULT_PORT))
        ("s,server","Server",cxxopts::value<std::string>()->default_value(DEFAULT_SERVER))
        ("chatty","Chatty",cxxopts::value<bool>()->default_value("false"))
        ("help", "Print help")
    ;
    auto result = options.parse(argc,argv);
    if (!result.count("nickname")) {
        std::cerr << "You must provide a nickname, refer to the help." << std::endl;
        std::cout << options.help({}) << std::endl;
        return 1;
    }
    if (result.count("help")) {
        std::cout << options.help({}) << std::endl;
        return 0;
    }
    try {
        nickname = result["nickname"].as<std::string>();
        server = result["server"].as<std::string>();
        port = result["port"].as<int>();
        chatty = result["chatty"].as<bool>();
    }
    catch(cxxopts::OptionException e) {
        std::cerr << "Option Error:" << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }
    if ((srv_socket = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        std::cerr << "Could not make socket" << std::endl;
        return -1;
    }
    // connect to server.
    memset(&srv_addr,0,sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(server.c_str());
    srv_addr.sin_port = htons(port);
    if (connect(srv_socket, (struct sockaddr *)&srv_addr, sizeof(struct sockaddr)) != 0) {
        std::cerr << "Could not connect to server" << std::endl;
        return -1;
    }
    msg = hello_msg(nickname);
    if (write(srv_socket,msg.c_str(),msg.length()) < 0) {
        std::cerr << "Failed to write hello message." << std::endl;
    }
    const int max_fd = srv_socket;
    std::cout << "Connected to " << server << std::endl;

    // do a select on STDIN and 
    Color::Modifier red(Color::FG_RED);
    Color::Modifier def(Color::FG_DEFAULT);

    // in the cryptographic case, use BIO_set_conn_ip
    while (true) {
        mk_client_fd_sets(srv_socket,&read_fds,&write_fds,&except_fds);
        int ret = select(max_fd + 1,&read_fds,&write_fds,&except_fds,NULL);
        switch (ret) {
            case -1:
                std::cerr << "Error in select" << std::endl;
                break;
            case 0:
                std::cerr << "Error in select" << std::endl;
                break;
            default:
                // this happens asynchronously as soon as typing starts.
                if (FD_ISSET(STDIN_FILENO,&read_fds)) {
                    msg = lock_msg(nickname);
                    write(srv_socket,msg.c_str(),msg.length());
                    // this blocks until a whole line of input. will be perceptible in human time (during which a lock occurs).
                    std::getline(std::cin,cmd);
                    try {
                        auto msg = parse_cmdline(cmd,nickname);
                        // this is either a countdown or write or goodbye message.
                        write(srv_socket,msg.c_str(),msg.length());
                    }
                    catch (const std::exception& e) {
                        break;
                    }
                    // used to be that the unlock message would be sent here, but that should be done after
                }
                if (FD_ISSET(srv_socket,&read_fds)) {
                    // okay i have confirmed.
                    recvloop(srv_socket,srv_readbuf);
                    try {
                        auto verified = validate_wrap(srv_readbuf);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Warning: the incoming message does not pass checks." << std::endl;
                    }
                    switch (classify(srv_readbuf)) {
                        case SHUTDOWN:
                            std::cout << "Server send shutdown message." << std::endl;
                            return 0;
                        case HELLO:
                            try {
                                peer_name = parse_hello(srv_readbuf);
                                std::cout << peer_name << " joined at " << now() << " on this clock and " << t_sent << " on theirs" << std::endl;
                                peer_names.push_back(peer_name);
                            }
                            catch(const std::exception& e) {
                                std::cerr << "Don't know who joined" << std::endl;
                            }
                            break;
                        case GOODBYE:
                            peer_name = parse_goodbye(srv_readbuf);
                            peer_names.erase(std::remove(peer_names.begin(),peer_names.end(),peer_name),peer_names.end());
                            std::cout << peer_name << " exited" << std::endl;
                            if (peer_names.size() == 0) {
                                std::cout << "You are the last one here" << std::endl;
                            }
                            break;
                        case PING_INIT:
                            try {
                                ping_n = parse_ping_init_msg(srv_readbuf);
                                if (chatty) { std::cout << "Set ping_n =" << ping_n << std::endl; }
                            }
                            catch (const std::exception& e ) {

                            }
                            break;
                        case PING: //this should only happen asynchronously for the first ping message, others are synchronous..
                            // very nice, ready to do implement client ping.
                            try {
                                seq = parse_ping_msg(srv_readbuf);
                                if (seq != 0) {

                                }
                                // reply back to the first ping.
                                write(srv_socket,srv_readbuf,strlen(srv_readbuf));
                            }
                            catch (const std::exception& e ) {

                            }
                            // we already have the first ping message in the buffer.
                            for(i=1;i<ping_n;++i) {
                                try {
                                    msglen_synch = recvloop(srv_socket,msgbuf_synch);
                                    seq = parse_ping_msg(msgbuf_synch);
                                    if (seq == i) { //echo it back.
                                        write(srv_socket,msgbuf_synch,msglen_synch);
                                    }
                                    else {
                                        std::cerr << " " << std::endl;
                                    }
                                }
                                catch (const std::exception& e ) {

                                }
                            }
                            break;
                        case PEER_LOCK:
                            std::cout << red;
                            break;
                        case PEER_UNLOCK:
                            std::cout << def;
                            break;
                        case WRITE_ARBITARY:
                            try {
                                cmdline = format_say_raw(parse_write_msg(srv_readbuf));
                                std::system(cmdline.c_str());
                            }
                            catch(const std::exception& e) {

                            }
                            break;
                        case COUNTDOWN_ORDER:
                            try {
                                parse_countdown_order_msg(srv_readbuf,countdown_start,countdown_delay);
                                cmdline = format_say_countdown(countdown_start,countdown_delay);
                                std::system(cmdline.c_str());
                            }
                            catch(const std::exception& e) {

                            }
                            break;
                        case MALFORMED:
                            std::cerr << "Recieved malformed message: " << srv_readbuf << std::endl;
                            break;
                        default:
                            std::cerr << "Should never happen, the classify function needs to be debugged" << std::endl;
                    }
                }
            }
    }
}
