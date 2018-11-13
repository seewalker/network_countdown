// In the client, writes should happen ASAP.
#include "countdown_client.hpp"


int main (int argc, char **argv) {

    std::string nickname,server;
    int port;
    bool chatty;
    cxxopts::Options options("./client","A partipant in a synchronized countdown.");
    // Parse Command Line Args
    options.add_options()
        ("n,nickname","Nickname you will be associated with.",cxxopts::value<std::string>())
        ("p,port","Port on which server is listening.",cxxopts::value<int>()->default_value(DEFAULT_PORT))
        ("s,server","Server IP address.",cxxopts::value<std::string>()->default_value(DEFAULT_SERVER))
        ("chatty","Chatty, whether or not to print logging messages",cxxopts::value<bool>()->default_value("false"))
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
    return countdown_client::interact(nickname,server,port,chatty);
}
