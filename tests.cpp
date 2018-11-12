#include "countdown_client.hpp"
#include "countdown_server.hpp"
#include "gtest/gtest.h"

// test to see if making and then parsing messages gets the expected values.

// EXPECT_EQ allows things to keep running, that's a nice default to start with.
// EXPECT_FLOAT_EQ, EXPECT_NE, ASSERT_STREQ, 
// during the MsgTest, check that the classify function works correctly.
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

TEST (CmdlineTest, Quit) {

}
TEST (CmdlineTest, Write) {

}
TEST (CmdlineTest, Countdown) {

}
TEST (DependencyTest, Say) {

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
}
