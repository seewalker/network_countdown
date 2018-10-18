CXXFLAGS=-std=c++17 -g

all: client server

client: countdown_client.cpp
	clang++ ${CXXFLAGS} countdown_client.cpp -o $@
server: countdown_server.cpp
	clang++ ${CXXFLAGS} countdown_server.cpp -o $@
