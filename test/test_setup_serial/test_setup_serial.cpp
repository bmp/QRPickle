// Setup over USB: the line protocol (services/setup_serial_proto). Page -> board lines start
// with "QRPSETUP "; the board answers READY, ACK or ERR and hands over the JSON on APPLY.
#include <unity.h>
#include <string.h>
#include "../../src/services/setup_serial_proto.h"

using namespace services::setup_proto;

void setUp() {}
void tearDown() {}

static void expect_reply(const Result& r, const char* text) {
    TEST_ASSERT_EQUAL_INT((int)Act::Reply, (int)r.act);
    TEST_ASSERT_EQUAL_STRING(text, r.reply);
}

void test_hello_gets_ready() {
    Proto p;
    expect_reply(p.line("QRPSETUP HELLO", 0), "QRPSETUP READY");
}

void test_chunks_in_order_give_the_json() {
    Proto p;
    expect_reply(p.line("QRPSETUP BEGIN 10", 0), "QRPSETUP ACK BEGIN");
    expect_reply(p.line("QRPSETUP DATA 0 {\"a\":", 10), "QRPSETUP ACK 0");
    expect_reply(p.line("QRPSETUP DATA 1 \"bc\"}", 20), "QRPSETUP ACK 1");
    const Result r = p.line("QRPSETUP APPLY", 30);
    TEST_ASSERT_EQUAL_INT((int)Act::Apply, (int)r.act);
    TEST_ASSERT_EQUAL_size_t(10, r.json_len);
    TEST_ASSERT_EQUAL_MEMORY("{\"a\":\"bc\"}", r.json, 10);
    TEST_ASSERT_EQUAL_INT('\0', r.json[10]);  // terminated for deserializeJson
    p.done();
    expect_reply(p.line("QRPSETUP BEGIN 2", 40), "QRPSETUP ACK BEGIN");  // a new transfer works
}

void test_data_spaces_are_kept() {
    Proto p;
    p.line("QRPSETUP BEGIN 8", 0);
    p.line("QRPSETUP DATA 0 {\"a\": ", 0);  // the text is everything after "<seq> ", spaces too
    p.line("QRPSETUP DATA 1 1}", 0);
    expect_reply(p.line("QRPSETUP DATA 2 ", 0), "QRPSETUP ACK 2");  // an empty chunk adds nothing
    const Result r = p.line("QRPSETUP APPLY", 0);
    TEST_ASSERT_EQUAL_INT((int)Act::Apply, (int)r.act);
    TEST_ASSERT_EQUAL_MEMORY("{\"a\": 1}", r.json, 8);
}

void test_order_errors() {
    Proto p;
    expect_reply(p.line("QRPSETUP DATA 0 x", 0), "QRPSETUP ERR seq");  // no BEGIN
    expect_reply(p.line("QRPSETUP APPLY", 0), "QRPSETUP ERR seq");
    p.line("QRPSETUP BEGIN 4", 0);
    expect_reply(p.line("QRPSETUP DATA 1 ab", 0), "QRPSETUP ERR seq");  // skipped 0
    expect_reply(p.line("QRPSETUP DATA 0 ab", 0), "QRPSETUP ERR seq");  // the transfer was reset
}

void test_length_errors() {
    Proto p;
    expect_reply(p.line("QRPSETUP BEGIN 2000", 0), "QRPSETUP ERR length");
    expect_reply(p.line("QRPSETUP BEGIN 0", 0), "QRPSETUP ERR length");
    p.line("QRPSETUP BEGIN 3", 0);
    expect_reply(p.line("QRPSETUP DATA 0 abcd", 0), "QRPSETUP ERR length");  // more than announced
    p.line("QRPSETUP BEGIN 5", 0);
    p.line("QRPSETUP DATA 0 abc", 0);
    expect_reply(p.line("QRPSETUP APPLY", 0), "QRPSETUP ERR length");  // fewer than announced
    char big[200] = "QRPSETUP DATA 0 ";
    memset(big + strlen(big), 'x', 161);
    p.line("QRPSETUP BEGIN 500", 0);
    expect_reply(p.line(big, 0), "QRPSETUP ERR length");  // a chunk over 160 B
}

void test_other_lines_are_ignored() {
    Proto p;
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.line("[Wi-Fi] Disconnected", 0).act);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.line("QRPSETUPX HELLO", 0).act);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.line("", 0).act);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.line(nullptr, 0).act);
    expect_reply(p.line("QRPSETUP NOPE", 0), "QRPSETUP ERR command");
}

void test_timeout_frees_the_transfer() {
    Proto p;
    p.line("QRPSETUP BEGIN 10", 1000);
    p.line("QRPSETUP DATA 0 abc", 5000);
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.tick(14000).act);  // 9 s since the last line
    expect_reply(p.tick(15100), "QRPSETUP ERR timeout");
    TEST_ASSERT_EQUAL_INT((int)Act::None, (int)p.tick(30000).act);  // reported once
    expect_reply(p.line("QRPSETUP DATA 1 de", 31000), "QRPSETUP ERR seq");
}

// v0.3.2: the page asks what the board detected (Set up over USB shows it and fills the report).
void test_info_command() {
    Proto p;
    const Result r = p.line("QRPSETUP INFO", 0);
    TEST_ASSERT_EQUAL_INT((int)Act::Info, (int)r.act);
    // Not in the middle of a transfer: the JSON would be lost.
    expect_reply(p.line("QRPSETUP BEGIN 10", 10), "QRPSETUP ACK BEGIN");
    expect_reply(p.line("QRPSETUP INFO", 20), "QRPSETUP ERR seq");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hello_gets_ready);
    RUN_TEST(test_chunks_in_order_give_the_json);
    RUN_TEST(test_data_spaces_are_kept);
    RUN_TEST(test_order_errors);
    RUN_TEST(test_length_errors);
    RUN_TEST(test_other_lines_are_ignored);
    RUN_TEST(test_timeout_frees_the_transfer);
    RUN_TEST(test_info_command);
    return UNITY_END();
}
