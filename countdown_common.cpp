/*
 * with the messages in these programs, a newline always indicates the end of a message.
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <vector>
#include <iostream>
#include <chrono>
#include "cxxopts.hpp"

enum msg_t {
    GREETING = 0,
    PEER_LOCK = 1,
    PEER_UNLOCK = 2,
    WRITE_ARBITARY = 3,
    COUNTDOWN_N = 4,
    COUNTDOWN_ORDER = 11,
    PING = 6,
    PING_INIT = 7,
    GOODBYE = 8,
    HELLO_ACK = 9,
    MALFORMED = 10
};

const int MAX_MSGLEN = 1024;
std::string validate_wrap(std::string x) {
    //check that only one newline, at the end.
    if (x[x.length()-1] != '\n') {
        throw std::invalid_argument("");
    }
    if (x.length() > MAX_MSGLEN) {
        throw std::length_error("");
    }
    return x;
}
// unit - seconds.
const double PING_TIMEOUT = 5;

//these are not messages that are typed by the human, but generated by the client and relayed by the server to other clients.
msg_t classify(char *buf) {
    std::string msg(buf);
    if (msg.find("hello") == 0) {
        return GREETING;
    }
    if (msg.find("hello_ack") == 0) {
        return HELLO_ACK;
    }
    if (msg.find("goodbye") == 0) {
        return GOODBYE;
    }
    if (msg == "lock") {
        return PEER_LOCK;
    }
    if (msg == "unlock") {
        return PEER_UNLOCK;
    }
    if (msg.find("write") == 0) {
        return WRITE_ARBITARY;
    }
    if (msg.find("countdown_n") == 0) {
        return COUNTDOWN_N;
    }
    else if (msg.find("countdown_order") == 0) {
        return COUNTDOWN_ORDER;
    }
    // only the server sends ping_init.
    else if (msg.find("init_ping") == 0) {
        return PING_INIT;
    }
    else if (msg.find("ping") == 0) {
        return PING;
    }
    else {
        return MALFORMED;
    }
}

std::string extract_between(std::string x,char c_begin,char c_end) {
    int i = 0,begin=-1,end=-1;
    for (char c : x) {
        if (c == c_end) {
            end = i;
            break;
        }
        if (c == c_begin) {
            begin = i;
        }
        i += 1;
    }
    if (begin > 0 && end > 0 && end > begin) {
        return x.substr(begin,end-begin);
    }
    else {
        throw std::invalid_argument("");
    }
}

int parse_unary_int(std::string target,std::string cmd) {
    int n;
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != target) {
        throw std::domain_error(" ");
    }
    iss >> n;
    return n;
}

std::string ping_msg(int seq) {
    std::ostringstream oss;
    oss << "ping " << seq << std::endl;
    return validate_wrap(oss.str());
}

int parse_ping_msg(std::string cmd) {
    return parse_unary_int("ping",cmd);
}

std::string write_msg(std::string arg) {
    std::ostringstream oss;
    oss << "write " << arg << std::endl;
    return validate_wrap(oss.str());
}

std::string parse_write_msg(std::string cmd) {
    std::string msg_t,arg;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != "write") {
        throw std::domain_error(" ");
    }
    std::getline(iss,arg,'\n');
    return arg;
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

// recv until newline.
int recvloop(int fd,char *buf) {
    int buf_ptr = 0,count;
    do {
        if ((count = recv(fd,buf + buf_ptr,MAX_MSGLEN,0)) < 0) {

        }
        buf_ptr += count;
        if (buf_ptr > MAX_MSGLEN) {

        }
    } while (buf[buf_ptr] != '\n');
    return buf_ptr;
}

std::time_t now( ) {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// for both lock and unlock.
void parse_lock_common(std::string target,std::string cmd,std::string &nickname,std::string &date_repr) {
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
    catch (const std::exception& e) {
        std::cerr << "Could not extract timestamp." << std::endl;
        date_repr = "";
    }
}

std::string parse_lock_msg(std::string cmd,std::string &date_repr) {
    std::string nickname;
    parse_lock_common("lock",cmd,nickname,date_repr);
    return nickname;
}

std::string parse_unlock_msg(std::string cmd,std::string &date_repr) {
    std::string nickname;
    parse_lock_common("unlock",cmd,nickname,date_repr);
    return nickname;
}