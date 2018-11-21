#include "countdown_client.hpp"
#include "countdown_server.hpp"
#include "gtest/gtest.h"
#include <unistd.h>

// test to see if making and then parsing messages gets the expected values.

// EXPECT_EQ allows things to keep running, that's a nice default to start with.
// EXPECT_FLOAT_EQ, EXPECT_NE, ASSERT_STREQ, 
// EXPECT_THROW allows for testing a given exception occuring on bad data.
// during the MsgTest, check that the classify function works correctly.

class MessageSuite : public ::testing::Test {
    protected:
        void SetUp() override {
            zero_msg = "";
            empty_msg = "\n";
            wrongToken_msg = "BOB -1\n";
            long_msg.assign(MAX_MSGLEN + 1,'*');
            long_msg = long_msg + '\n';
            multiline_msg = "abc\n123\nboblawlawlawblog\n";
        }
        void TearDown() override {

        }
        std::string zero_msg;
        std::string long_msg;
        std::string multiline_msg;
        std::string empty_msg;
        std::string wrongToken_msg;
};

TEST (MsgTest, Lock) {
    std::string msg,date_repr,nick="alex";
    msg = lock_msg(nick);
    EXPECT_EQ(classify(msg.c_str()),PEER_LOCK);
    EXPECT_STREQ(parse_lock_msg(msg,date_repr).c_str(),nick.c_str());
}
TEST (MsgTest, Unlock) {
    std::string msg,date_repr,nick="alex",parsed_nick;
    msg = unlock_msg(nick);
    EXPECT_EQ(classify(msg.c_str()),PEER_UNLOCK);
    parsed_nick = parse_unlock_msg(msg,date_repr);
    EXPECT_STREQ(parsed_nick.c_str(),nick.c_str());
}
TEST (MsgTest, Countdown_n) {

}
TEST (MsgTest, Ping_init) {

}
TEST (MsgTest, Write) {
    std::string msg,arg="ready to start?",parsed_arg;
    msg = write_msg(arg);
    EXPECT_EQ(classify(msg.c_str()),WRITE_ARBITRARY);
    parsed_arg = parse_write_msg(msg);
    EXPECT_EQ(arg.c_str(),parsed_arg.c_str());
}
TEST (MsgTest, Countdown_order) {

}
TEST (MsgTest, Ping) {
    int seq=4,parsed_seq;
    std::string msg,nick="rachael";
    msg = ping_msg(seq);
    EXPECT_EQ(classify(msg.c_str()),PING);
    parsed_seq = parse_ping_msg(msg);
    EXPECT_EQ(seq,parsed_seq);
}

TEST_F (MessageSuite,ValidateWrap) {
    EXPECT_THROW(validate_wrap(multiline_msg),std::invalid_argument);
    EXPECT_THROW(validate_wrap(zero_msg),std::invalid_argument);
    EXPECT_THROW(validate_wrap(empty_msg),std::invalid_argument);
    EXPECT_THROW(validate_wrap(long_msg),std::invalid_argument);
    EXPECT_THROW(validate_wrap(wrongToken_msg),std::invalid_argument);
}
// the member variables are accessible here.
TEST_F (MessageSuite,Ping) {
    std::cout << "Determining if SetUp ran as expected by printing out multiline message" << std::endl << multiline_msg;
    std::string negative_seq_msg = ping_msg(-1); //validate_wrap will not catch this, catch the exception in the parse.
    EXPECT_THROW(parse_ping_msg(negative_seq_msg),std::domain_error);
}

TEST (CmdlineTest, Quit) {

}
TEST (CmdlineTest, Write) {

}
TEST (CmdlineTest, Countdown) {

}

TEST (InteractiveTest,Write) {

}

// in the test, server doesn't really need stdin, so the stdin is of the client child process.
// the server's standard output can also be unliked, and just having the client stdout.
TEST (DISABLED_InteractiveTest,Countdown) {
    const std::string SERVER = "127.0.0.1", NICKNAME = "alex";
    const int PORT = 9050;
    const bool CHATTY = false;
    const int PING_N = 3;
    // need to start child processes, and manage pipes to them.
    // this should 
    int client_pipes[2],pid;
    const int PIPE_READ = 0;
    const int PIPE_WRITE = 1;
    if (pipe(client_pipes) < 0) {
        perror("allocating pipe for client child process failed");
        FAIL();
        return;
    }
    pid = fork();
    if (pid == 0) { //is the client child process.
        if (dup2(client_pipes[PIPE_READ],STDIN_FILENO) == -1) {
            FAIL();
            return;
        }
        if (dup2(client_pipes[PIPE_WRITE],STDOUT_FILENO) == -1) {
            FAIL();
            return;
        }
        if (dup2(client_pipes[PIPE_WRITE],STDERR_FILENO) == -1) {
            FAIL();
            return;
        }
        countdown_client::interact(NICKNAME,SERVER,PORT,CHATTY);
   }
    else {
        pid = fork();
        if (pid == 0) { //is the server child process.
            countdown_server::interact(SERVER,PORT,PING_N);
        }
        else { //is the parent process.

        }
    }
}

TEST (DependencyTest, Say) {
    EXPECT_EQ(std::system("say hi"),0);
    EXPECT_EQ(std::system("say hi to your mom"),0);
}

// these interactive tests can be ignored by running "./test --gtest_filter=-*InteractiveTest*"
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
}
