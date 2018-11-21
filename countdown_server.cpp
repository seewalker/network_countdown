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
#include "countdown_server.hpp"

// maybe do some testing on the server runtime.
#ifdef __has_include
    #if __has_include("gtest/gtest.h")
        #define USE_GTEST 1
    #else
        #define USE_GTEST 0
    #endif
#else
    #define USE_GTEST 0
#endif

/* Here's how pings work.
 *  n rounds of blocking writes and recvs on the current file descriptor.
 *  server seq t_sent_serverclock
 *      .(keep a clock going). when i get a message back, that is a round trip.
 *  client seq t_recv_clientclock t_sent_clientclock.
 *      this gives the halfway-between time on the client's clock.
 */
// maybe for now there can be the modeling assumption that one trip is half an rtt.

// server decides on unlock after countdown or write has occured.
// when should a shutdown occur?


int main(int argc, char **argv) {
    std::string server;
    int port,ping_n;
    cxxopts::Options options("./server","A program that will coordinate a countdown.");
    // Tne number of pings applies both to the before and after of a countdown (the first one, to get up-to-date estimates on latencies. the latter to see how close it was to a successful countdown.).
    options.add_options()
        ("p,port","Port on which to listen.",cxxopts::value<int>()->default_value(DEFAULT_PORT))
        ("n,ping_n","Number of ping messages to send when estimating round trip times.",cxxopts::value<int>()->default_value(DEFAULT_PING_N))
        ("s,server","Server IP address.",cxxopts::value<std::string>()->default_value(DEFAULT_SERVER))
        ("help", "Print help")
    ;
    auto result = options.parse(argc,argv);
    if (result.count("help")) {
        std::cout << options.help({}) << std::endl;
        return 0;
    }
    try {
        port = result["port"].as<int>();
        ping_n = result["ping_n"].as<int>();
        server = result["server"].as<std::string>();
    }
    catch(cxxopts::OptionException e) {
        std::cerr << "Option Error:" << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return countdown_server::interact(server,port,ping_n);
}
