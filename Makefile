CXXFLAGS=-std=c++17 -g
client: countdown_client.cpp
	clang++ ${CXXFLAGS} countdown_client.cpp -o $@
server: countdown_server.cpp
	clang++ ${CXXFLAGS} countdown_server.cpp -o $@
all: client server
