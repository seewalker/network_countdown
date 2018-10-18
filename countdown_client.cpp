// In the client, writes should happen ASAP.
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "cxxopts.hpp"
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include "countdown_common.cpp"

#define MAX_MSGLEN 4096

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

std::string format_say_cmd(int n) {
    std::ostringstream oss;
    for(;n>0;--n) {
        oss << "say " << n << " && sleep 1 && ";
    }
    oss << "say play";
    return oss.str();
}

std::string format_say_cmd(std::string saying) {
    std::ostringstream oss;
    oss << "say ";
    oss << saying;
    return oss.str();
}

std::time_t now( ) {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

std::string validate_wrap(std::string x) {
    //check that only one newline, at the end.
    //check that length is less than constant max.
    if (x.length() > MAX_MSGLEN) {
        throw std::length_error("");
    }
}

std::string hello_msg(std::string nickname) {
   std::ostringstream oss;
   oss << "hello " << nickname << std::endl;
   return validate_wrap(oss.str());
}

std::string goodbye_msg(std::string nickname) {
   std::ostringstream oss;
   oss << "goodbye " << nickname << std::endl;
   return validate_wrap(oss.str());
}

std::string parse_greeting_common(std::string target,std::string cmd) {
    std::string msg_t,nickname;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != target) {
        throw std::domain_error(" ");
    }
    iss >> nickname;
    return nickname;
}

std::string parse_hello(std::string cmd ) {
    return parse_greeting_common("hello",cmd);
}

std::string parse_goodbye(std::string cmd) {
    return parse_greeting_common("goodbye",cmd);
}

std::string lock_msg(std::string nickname) {
   std::ostringstream oss;
   auto t = now();
   oss << "lock " << nickname << " " << "(" << std::ctime(&t) << ")" << std::endl;
   return validate_wrap(oss.str());
}

std::string unlock_msg(std::string nickname) {
   std::ostringstream oss;
   auto t = now();
   oss << "unlock " << nickname << " " << "(" << std::ctime(&t) << ")" << std::endl;
   return validate_wrap(oss.str());
}

// for both lock and unlock.
void parse_lock_common(std::string target,std::string &nickname,std::string &date_repr) {
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    size_t begin_date,end_date,i=0;
    if (msg_t != target) {
        throw std::domain_error(" ");
    }
    iss >> nickname;
    try {
        date_repr = extract_between(iss.str(),'(',')');
    }
    catch ( ) {
        std::cerr << "Could not extract timestamp." << std::endl;
        date_repr = "";
    }
}

std::string parse_lock_msg(std::string cmd,std::string &date_repr) {
    std::string nickname;
    parse_lock_common("lock",nickname,date_repr);
}

std::string parse_unlock_msg(std::string cmd,std::string &date_repr) {
    std::string nickname;
    parse_lock_common("unlock",nickname,date_repr);
}

std::string countdown_msg(int start_idx) {
    std::ostringstream oss;
    oss << "countdown " << start_idx << std::endl;
    return validate_wrap(oss.str());
}

int parse_countdown_msg(std::string cmd) {
    return parse_unary_int("countdown",cmd);
}


// this is parsing what is coming in over the command line.
std::string parse_cmdline(std::string buf) {
    std::string word,arg,junk;
    std::istringstream iss(buf);
    int start_idx;
    if (buf.find("countdown") == 0) {
        if (std::getline(iss,junk,' ')) {
            std::getline(iss,arg,'\n');
            iss >> start_idx;
        }
        else {
            start_idx = 3;
        }
        return countdown_msg(start_idx); 
    }
    else if (buf.find("write") == 0) {
        if (std::getline(iss,junk,' ')) {
            std::getline(iss,arg,'\n');
        }
        else {
            throw std::invalid_argument("Invalid syntax");
        }
        return write_msg(arg);
    }
    else if (buf.find("q") == 0) {
        return goodbye_msg();
    }
    else {
        throw std::invalid_argument("Invalid syntax");
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
    std::string nickname,server,cmd,write_buffer,srv_buffer,msg,t_sent;
    int port,srv_socket,count,msglen,ping_init_n=-1;
    std::vector<std::string> peer_names;
    struct sockaddr_in srv_addr;
    fd_set read_fds,write_fds,except_fds;
    char srv_readbuf[MAX_MSGLEN];
    int srv_readbuf_ptr = 0;
    bool peer_writing = false;

    cxxopts::Options options("Countdown Client","A program that will coordinate a countdown.");
    // Parse Command Line Args
    options.add_options()
        ("n,nickname","Nickname",cxxopts::value<std::string>())
        ("p,port","Port",cxxopts::value<int>())
        ("s,server","Server",cxxopts::value<std::string>())
    ;
    auto result = options.parse(argc,argv);
    try {
        nickname = result["nickname"].as<std::string>();
        server = result["server"].as<std::string>();
        port = result["port"].as<int>();
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
    const int max_fd = srv_socket;
    std::cout << "Connected to " << server << std::endl;
    // do a select on STDIN and 
    Color::Modifier red(Color::FG_RED);
    Color::Modifier def(Color::FG_DEFAULT);
    while (true) {
        mk_client_fd_sets(srv_socket,write_buffer,&read_fds,&write_fds,&except_fds);
        int ret = select(max_fd + 1,&read_fds,&write_fds,&except_fds,NULL);
        switch (ret) {
            case -1:
                std::cerr << "Error in select" << std::endl;
                break;
            case 0:
                std::cerr << "Error in select" << std::endl;
                break;
            default:
                if (FD_ISSET(STDIN_FILENO,&read_fds)) {
                    msg = lock_msg(nickname);
                    write(srv_socket,msg.c_str(),msg.length());
                    std::getline(std::cin,cmd);
                    try {
                        auto msg = parse_cmd(cmd);
                        // this is either a countdown or write or goodbye message.
                        write(srv_socket,msg.c_str(),msg.length());
                    }
                    catch (const std::exception& e) {
                        break;
                    }
                    msg = unlock_msg(nickname);
                    write(srv_socket,msg.c_str(),msg.length());
                }
                if (FD_ISSET(srv_socket,&read_fds)) {
                    if ((count = recv(srv_socket,srv_readbuf_ptr + srv_readbuf,MAX_MSGLEN,0)) < 0) {
                        std::cerr << "recv failed" << std::endl;
                    }
                    if (srv_readbuf[count - 1] != '\n') {
                        srv_readbuf_ptr += count;
                    }
                    else {
                        switch (classify(srv_readbuf)) {
                            case GREETING:
                                try {
                                    auto peer_name = parse_greeting(srv_readbuf,t_sent);
                                    std::cout << peer_name << " joined at " << now() << " on this clock and " << t_sent << " on theirs" << std::endl;
                                    peer_names.push_back(peer_name);
                                }
                                catch(const std::exception& e) {
                                    std::cerr << "Don't know who joined" << std::endl;
                                }
                                break;
                            case GOODBYE:
                                auto peer_name = parse_goodbye(srv_readbuf);
                                peer_names.erase(std::remove(peer_names.begin(),peer_names.end(),peer_name),peer_names.end());
                                std::cout << peer_name << " exited" << std::endl;
                                if (peer_names.size() == 0) {
                                    std::cout << "You are the last one here" << std::endl;
                                }
                                break;
                            case PING_INIT:
                                break;
                            case PING: //this should only happen asynchronously for the first ping message, others are synchronous..

                                break;
                            case PEER_LOCK:
                                std::cout << red;
                                break;
                            case PEER_UNLOCK:
                                std::cout << def;
                                break;
                            case WRITE_ARBITARY:
                                try {
                                    auto cmdline = format_say_cmd(parse_write_arg(srv_readbuf));
                                    std::system(cmdline.c_str());
                                }
                                catch(const std::exception& e) {

                                }
                                break;
                            case COUNTDOWN_N:
                                try {
                                    auto cmdline = format_say_cmd(parse_countdown_n(srv_readbuf));
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
                                break;
                        }
                    }
                }
        }
    }
}
