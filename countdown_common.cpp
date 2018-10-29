/*
 * with the messages in these programs, a newline always indicates the end of a message.
 */
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <system_error>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <vector>
#include <iostream>
#include <chrono>
#include "cxxopts.hpp"

#define DEFAULT_SERVER "45.33.122.23"
#define DEFAULT_PORT "9573"
#define DEFAULT_PING_N "3"
const int DEFAULT_COUNTDOWN_N = 3;

enum msg_t {
    HELLO = 0,
    PEER_LOCK = 1,
    PEER_UNLOCK = 2,
    WRITE_ARBITARY = 3,
    COUNTDOWN_N = 4,
    PING = 6,
    PING_INIT = 7,
    GOODBYE = 8,
    HELLO_ACK = 9,
    MALFORMED = 10,
    COUNTDOWN_ORDER = 11,
    COUNTDOWN_INTERRUPT = 12,
    SHUTDOWN = 13
};

const int MAX_MSGLEN = 1024;
// unit - seconds.
const double PING_TIMEOUT = 5;

std::time_t now( ) {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

//these are not messages that are typed by the human, but generated by the client and relayed by the server to other clients.
msg_t classify(const char *buf) {
    std::string msg(buf);
    if (msg.find("hello") == 0) { return HELLO; }
    else if (msg.find("hello_ack") == 0) { return HELLO_ACK; }
    else if (msg.find("goodbye") == 0) { return GOODBYE; }
    else if (msg.find("lock") == 0) { return PEER_LOCK; }
    else if (msg.find("unlock") == 0) { return PEER_UNLOCK; }
    else if (msg.find("write") == 0) { return WRITE_ARBITARY; }
    else if (msg.find("countdown_n") == 0) { return COUNTDOWN_N; }
    else if (msg.find("countdown_order") == 0) { return COUNTDOWN_ORDER; }
    else if (msg.find("countdown_interrupt") == 0) { return COUNTDOWN_INTERRUPT; }
    // only the server sends ping_init.
    else if (msg.find("init_ping") == 0) { return PING_INIT; }
    else if (msg.find("ping") == 0) { return PING; }
    else if (msg.find("shutdown") == 0) { return SHUTDOWN; }
    else { return MALFORMED; }
}


// checks formatting problems with messages.
// this can only throw invalid_argument.
std::string validate_wrap(std::string x) {
    //check that only one newline, at the end.
    if (classify(x.c_str()) == MALFORMED) {
        throw std::invalid_argument("validate_wrap error : Message not of known type");
    }
    if (x[x.length()-1] != '\n') {
        throw std::invalid_argument("validate_wrap error : Does not end in newline");
    }
    if (x.length() > MAX_MSGLEN) {
        throw std::invalid_argument("validate_wrap error : Message length exceeds limit");
    }
    if (std::count(x.begin(),x.end(),'\n') > 1) {
        throw std::invalid_argument("validate_wrap error : Message has multiple newlines.");
    }
    return x;
}

std::string extract_between(std::string x,char c_begin,char c_end) {
    int i = 0,begin=-1,end=-1;
    for (char c : x) {
        if (c == c_end) {
            end = i;
            break;
        }
        if (c == c_begin) { begin = i; }
        i += 1;
    }
    if (begin > 0 && end > 0 && end > begin) { return x.substr(begin,end-begin); }
    else { throw std::invalid_argument(""); }
}

// only defined for strings where the unary int is non-negative (the negative is the way the error is detected.)
int parse_unary_int(std::string target,std::string cmd) {
    int n=-1;
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != target) { throw std::domain_error("parse_unary_int: Message not of expected type."); }
    iss >> n;
    if (n < 0) { throw std::domain_error("parse_unary_int: Could not extract the int value expected."); }
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

std::string lock_common_msg(std::string msg_t, std::string nickname) {
   std::ostringstream oss;
   auto t = now();
   char *t_repr = std::ctime(&t);
   // remove trailing newline.
   t_repr[strlen(t_repr)-1] = '\0';
   oss << msg_t << " " << nickname << " " << "(" << t_repr << ")" << std::endl;
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
    if (msg_t != target) { throw std::domain_error("Message type is not what is expected."); }
    iss >> nickname;
    return nickname;
}

std::string parse_hello(std::string cmd ) {
    return parse_greeting_common("hello",cmd);
}

std::string parse_goodbye(std::string cmd) {
    return parse_greeting_common("goodbye",cmd);
}

int recvloop(int fd,char *buf) {
    int buf_ptr = 0,count,i;
    // since the buffers may be reused, zero it out between.
    for (i=0;i<MAX_MSGLEN;++i) {
        buf[i] = '\0';
    }
    do {
        if ((count = recv(fd,buf + buf_ptr,MAX_MSGLEN,0)) < 0) {
            perror("recvloop error: ");
            throw std::system_error(errno,std::generic_category());
        }
        buf_ptr += count;
        if (buf_ptr > MAX_MSGLEN) {
            throw std::length_error(" ");
        }
    } while (buf[buf_ptr-1] != '\n' && buf[buf_ptr] != '\0');
    return buf_ptr;
}


// for both lock and unlock.
void parse_lock_common(std::string target,std::string cmd,std::string &nickname,std::string &date_repr) {
    std::string msg_t;
    std::istringstream iss(cmd);
    iss >> msg_t;
    if (msg_t != target) { throw std::domain_error("Message type is not what is expected."); }
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

int parse_countdown_n_msg(std::string cmd) {
    return parse_unary_int("countdown_n",cmd);
}
