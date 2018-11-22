UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
CXX=clang++-6.0
ARCH_FLAGS=-lpthread
else
CXX=clang++
ARCH_FLAGS=
endif
CXXFLAGS=-std=c++17 -g -Wall ${ARCH_FLAGS}

# the "?=" operator allows environment variables or command line variables to "make" to override these default values.
# default to using SSL if the openssl command line program exists.
OPENSSL_FOUND = $(openssl no-command)
ifeq ($(OPENSSL_FOUND),0)
	USE_SSL ?= 1
else
	USE_SSL ?= 0
endif

MACROS=-DUSE_SSL=$(USE_SSL)
TARGETS=x86_64-unknown-linux-elf

all: client server

client: countdown_client.cpp
	echo "USE_SSL=${USE_SSL}"
	${CXX} ${CXXFLAGS} ${MACROS} countdown_client.cpp -o $@
server: countdown_server.cpp
	echo "USE_SSL=${USE_SSL}"
	${CXX} ${CXXFLAGS} ${MACROS} countdown_server.cpp -o $@
test: tests.cpp
	${CXX} ${CXXFLAGS} ${MACROS} tests.cpp -lgtest -o $@
release:
	$(foreach platform,$(TARGETS),${CXX} ${CXXFLAGS} ${MACROS} countdown_client.cpp -target $(platform) -o build/client_$(platform);)
	$(foreach platform,$(TARGETS),${CXX} ${CXXFLAGS} ${MACROS} countdown_server.cpp -target $(platform) -o build/server_$(platform);)
all: client server

clean:
	rm client server test
