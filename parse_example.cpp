#include <iostream>
#include <string>
#include <sstream>

std::string countdown_msg(int start_idx) {
    std::ostringstream oss;
    oss << "countdown " << start_idx;
    return oss.str();
}

int parse_countdown_n(std::string cmd) {
    int n;
    std::string junk,arg;
    std::istringstream iss(cmd);
    if (std::getline(iss,junk,' ')) {
        std::getline(iss,arg,'\n');
        iss >> n;
        return n;
    }
    return -1;
}

int p2(std::string cmd) {
    std::istringstream iss(cmd);
    std::string junk;
    int n;
    iss >> junk;
    iss >> n;
    return n;
}

int main( ) {
    std::string msg = countdown_msg(3);
    std::cout << msg << std::endl;
    int n = p2(msg);
    std::cout << n << std::endl;
}
