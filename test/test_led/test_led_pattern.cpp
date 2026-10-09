// The status LED patterns documented in docs/LEDColours.md (host test of src/hw/led_pattern).
#include <unity.h>
#include "../../src/hw/led_pattern.h"
#include "../../src/hw/led_pattern.cpp"  // host-safe; compiled into this test only

using namespace hw::led_rgb;

void setUp() {}
void tearDown() {}

static void assert_rgb(uint8_t r, uint8_t g, uint8_t b, Rgb c, const char* what) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(r, c.r, what);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(g, c.g, what);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(b, c.b, what);
}

static Rgb at(LEDState s, uint32_t now) { return pattern_color(s, now, -1, -1); }

void test_boot_sequence_colours() {
    assert_rgb(180, 45, 0, at(STATE_BOOT_HW, 0), "hardware init: solid amber");
    assert_rgb(180, 45, 0, at(STATE_BOOT_HW, 12345), "amber is steady");
    assert_rgb(0, 0, 150, at(STATE_BOOT_WIFI, 777), "network search: solid blue");
    assert_rgb(0, 25, 0, at(STATE_BOOT_READY, 0), "ready: dim green");
    assert_rgb(0, 0, 0, at(STATE_OFF, 500), "standby: off");
}

void test_disabled_breathing_states_are_off() {
    for (uint32_t t = 0; t < 5000; t += 250) {
        assert_rgb(0, 0, 0, at(STATE_BOOT_SYNC, t), "services sync: off (breathing cyan disabled)");
        assert_rgb(0, 0, 0, at(STATE_WIFI_LOST, t), "link lost: off (breathing magenta disabled)");
    }
}

void test_fault_is_three_red_flashes_every_3_seconds() {
    int flashes = 0;
    bool was_on = false;
    for (uint32_t t = 0; t < 3000; t += 10) {
        Rgb c = at(STATE_FAULT, t);
        bool on = c.r == 200 && c.g == 0 && c.b == 0;
        if (!on) assert_rgb(0, 0, 0, c, "fault: dark between flashes");
        if (on && !was_on) flashes++;
        if (t >= 600) TEST_ASSERT_FALSE_MESSAGE(on, "fault: dark for the rest of the 3 s cycle");
        was_on = on;
    }
    TEST_ASSERT_EQUAL_INT(3, flashes);
    assert_rgb(200, 0, 0, at(STATE_FAULT, 3000), "fault: cycle repeats");
}

void test_priority_strobe_alternates_white_magenta_for_3_seconds() {
    assert_rgb(120, 120, 120, pattern_color(STATE_OFF, 0, 0, -1), "strobe: white");
    assert_rgb(150, 0, 150, pattern_color(STATE_OFF, 125, 125, -1), "strobe: magenta after 125 ms");
    assert_rgb(120, 120, 120, pattern_color(STATE_OFF, 250, 250, -1), "strobe: white again");
    assert_rgb(120, 120, 120, pattern_color(STATE_OFF, 3000, 3000, -1), "strobe: still on at 3 s");
    assert_rgb(0, 0, 0, pattern_color(STATE_OFF, 3001, 3001, -1), "strobe: over after 3 s");
}

void test_traffic_pulse_is_30ms_of_faint_cyan() {
    assert_rgb(0, 40, 60, pattern_color(STATE_OFF, 10, -1, 0), "pulse: faint cyan");
    assert_rgb(0, 40, 60, pattern_color(STATE_OFF, 40, -1, 30), "pulse: lasts 30 ms");
    assert_rgb(0, 0, 0, pattern_color(STATE_OFF, 41, -1, 31), "pulse: over after 30 ms");
}

void test_priorities_strobe_over_pulse_over_state() {
    assert_rgb(120, 120, 120, pattern_color(STATE_FAULT, 0, 0, 0), "strobe wins over pulse and fault");
    assert_rgb(0, 40, 60, pattern_color(STATE_BOOT_WIFI, 0, -1, 0), "pulse wins over state");
    assert_rgb(0, 0, 150, pattern_color(STATE_BOOT_WIFI, 0, 5000, 100), "expired effects show the state");
}

void test_common_anode_inversion() {
    TEST_ASSERT_EQUAL_UINT8(255, pwm_duty(0));   // off
    TEST_ASSERT_EQUAL_UINT8(0, pwm_duty(255));   // fully on
    TEST_ASSERT_EQUAL_UINT8(75, pwm_duty(180));  // amber red channel
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_sequence_colours);
    RUN_TEST(test_disabled_breathing_states_are_off);
    RUN_TEST(test_fault_is_three_red_flashes_every_3_seconds);
    RUN_TEST(test_priority_strobe_alternates_white_magenta_for_3_seconds);
    RUN_TEST(test_traffic_pulse_is_30ms_of_faint_cyan);
    RUN_TEST(test_priorities_strobe_over_pulse_over_state);
    RUN_TEST(test_common_anode_inversion);
    return UNITY_END();
}
