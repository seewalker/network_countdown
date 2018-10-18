UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
CXX=clang++-6.0
else
CXX=clang++
endif 
CXXFLAGS=-std=c++17 -g

all: client server

client: countdown_client.cpp
	${CXX} ${CXXFLAGS} countdown_client.cpp -o $@
server: countdown_server.cpp
	${CXX} ${CXXFLAGS} countdown_server.cpp -o $@
all: client server
