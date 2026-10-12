#pragma once
#include <unity.h>
#include "../../src/services/space_parse.h"

// Feed elements in the shape tools/space_feed/build_feed.py writes (hand-made sample).
static const char* SPACE_SAT = R"({"id":25544,"n":"ISS","c":"fsv",
 "t1":"1 25544U 98067A   26282.82140208  .00006479  00000+0  12652-3 0  9999",
 "t2":"2 25544  51.6314  92.7969 0006774 242.6225 117.4075 15.48793003589525",
 "tx":[{"d":145800000,"u":144490000,"m":"FM","x":"crew R2+3","c":"f"},{"d":437550000,"m":"SSTV","c":"s"},
       {"d":145825000,"u":145825000,"m":"AFSK","c":"a"},{"d":1,"c":"f"},{"d":2,"c":"f"}]})";

inline void test_space_parse_sat() {
    using namespace services::space;
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, SPACE_SAT));
    Sat s;
    TEST_ASSERT_TRUE(parse_sat(doc.as<JsonObjectConst>(), s));
    TEST_ASSERT_EQUAL_UINT32(25544, s.norad);
    TEST_ASSERT_EQUAL_STRING("ISS", s.name);
    TEST_ASSERT_EQUAL_HEX8(CAT_FM | CAT_SSTV | CAT_VOICE, s.categories);
    TEST_ASSERT_EQUAL_UINT8(MAX_TX, s.tx_count);  // capped
    TEST_ASSERT_EQUAL_UINT32(144490000, s.tx[0].up_hz);
    TEST_ASSERT_EQUAL_STRING("SSTV", s.tx[1].mode);
    TEST_ASSERT_EQUAL_UINT8(CAT_SSTV, s.tx[1].category);

    TEST_ASSERT_FALSE(deserializeJson(doc, R"({"id":1,"n":"BAD","t1":"1 short","t2":"2 short"})"));
    TEST_ASSERT_FALSE(parse_sat(doc.as<JsonObjectConst>(), s));
}

inline void test_space_parse_event() {
    using namespace services::space;
    JsonDocument doc;
    Event e;
    TEST_ASSERT_FALSE(deserializeJson(doc, R"({"k":"sstv","s":1000,"e":5000,"t":"SSTV Series 33","x":"437.550 MHz FM, Robot 36","f":437550000,"m":"Robot 36"})"));
    TEST_ASSERT_TRUE(parse_event(doc.as<JsonObjectConst>(), e));
    TEST_ASSERT_EQUAL(EVENT_SSTV, e.kind);
    TEST_ASSERT_EQUAL_UINT32(437550000, e.freq_hz);
    TEST_ASSERT_FALSE(deserializeJson(doc, R"({"k":"party","s":1,"e":2})"));
    TEST_ASSERT_FALSE(parse_event(doc.as<JsonObjectConst>(), e));
    TEST_ASSERT_FALSE(deserializeJson(doc, R"({"k":"contact","s":5000,"e":1000})"));
    TEST_ASSERT_FALSE(parse_event(doc.as<JsonObjectConst>(), e));  // ends before it starts
}

inline void test_space_insert_pass_keeps_earliest() {
    using namespace services::space;
    Pass list[3];
    size_t n = 0;
    for (uint32_t aos : {500u, 100u, 300u, 200u, 900u, 50u}) {
        Pass p = {};
        p.aos = aos;
        n = insert_pass(list, n, 3, p);
    }
    TEST_ASSERT_EQUAL(3, n);
    TEST_ASSERT_EQUAL_UINT32(50, list[0].aos);
    TEST_ASSERT_EQUAL_UINT32(100, list[1].aos);
    TEST_ASSERT_EQUAL_UINT32(200, list[2].aos);
}

inline void test_space_event_passes_and_alert() {
    using namespace services::space;
    SatInfo sats[2] = {};
    sats[0].norad = ISS;
    sats[1].norad = 27607;
    Event ev[2] = {};
    ev[0].kind = EVENT_SSTV;
    ev[0].start = 10000;
    ev[0].end = 20000;
    ev[1].kind = EVENT_CONTACT;
    ev[1].start = 30000;
    ev[1].end = 30600;
    Pass p[4] = {};
    p[0] = {0, 9500, 9800, 10100, 40, 300, 120, 0};  // ISS, overlaps the SSTV window start
    p[1] = {0, 25000, 25300, 25600, 20, 200, 30, 0};  // ISS, no event
    p[2] = {1, 15000, 15300, 15600, 50, 10, 190, 0};  // SO-50 in the SSTV window: not the ISS
    p[3] = {0, 29900, 30200, 30500, 60, 250, 80, 0};  // ISS during the contact
    mark_event_passes(p, 4, sats, ev, 2);
    TEST_ASSERT_EQUAL_HEX8(PASS_SSTV_EVENT, p[0].flags);
    TEST_ASSERT_EQUAL_HEX8(0, p[1].flags);
    TEST_ASSERT_EQUAL_HEX8(0, p[2].flags);
    TEST_ASSERT_EQUAL_HEX8(PASS_CONTACT, p[3].flags);

    const uint32_t lead = 30 * 60;
    TEST_ASSERT_TRUE(alert_state(p, 4, 9500 - lead - 1, lead) == Alert::NONE);  // too early
    TEST_ASSERT_TRUE(alert_state(p, 4, 9500 - lead, lead) == Alert::SOON);
    TEST_ASSERT_TRUE(alert_state(p, 4, 9600, lead) == Alert::NOW);
    TEST_ASSERT_TRUE(alert_state(p, 4, 25100, lead) == Alert::NONE);  // a pass without an event
    TEST_ASSERT_TRUE(alert_state(p, 4, 9600, 0) == Alert::NONE);  // alerts off

    TEST_ASSERT_EQUAL_STRING("N", compass(0));
    TEST_ASSERT_EQUAL_STRING("NW", compass(315));
    TEST_ASSERT_EQUAL_STRING("SSE", compass(160));
    TEST_ASSERT_EQUAL_STRING("N", compass(355));
}
