CXX=clang++-6.0
CXXFLAGS=-std=c++17 -g
client: countdown_client.cpp
	clang++-6.0 ${CXXFLAGS} countdown_client.cpp -o $@
server: countdown_server.cpp
	clang++-6.0 ${CXXFLAGS} countdown_server.cpp -o $@
all: client server
