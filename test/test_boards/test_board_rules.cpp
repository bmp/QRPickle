// Board selection rules (src/hw/board_rules.cpp): fixture boards plus checks on the real list.
#include <unity.h>
#include <cstring>
#include "../../src/hw/board_rules.h"
#include "../../src/hw/board_rules.cpp"  // host-safe; compiled into this test only
#include "../../src/hw/boards_gen.h"

using namespace hw;
using namespace hw::rules;

void setUp() {}
void tearDown() {}

static BoardDef fixture(const char* id, uint32_t rddid, uint32_t rdid4, int8_t bl, TouchType touch, uint8_t t_addr = 0) {
    BoardDef b{};
    b.id = id; b.model = id; b.name = id; b.status = BoardStatus::Mapped; b.complete = true;
    b.init = PanelInit::ILI9341; b.rddid = rddid; b.rdid4 = rdid4;
    b.width = 320; b.height = 240; b.swap_xy = true; b.bgr = true;
    b.sclk = 14; b.mosi = 13; b.miso = 12; b.cs = 15; b.dc = 2;
    b.bl_pin = bl; b.bl_active_high = true;
    b.led_r = 4; b.led_g = 16; b.led_b = 17; b.led_active_low = true;
    b.touch = touch; b.t_cs = 33; b.t_irq = 36; b.t_addr = t_addr;
    b.t_sda = t_addr ? 33 : NO_PIN; b.t_scl = t_addr ? 32 : NO_PIN; b.t_rst = t_addr ? 25 : NO_PIN;
    b.i2c_sda = 27; b.i2c_scl = 22; b.ldr_gpio = 34; b.sd_cs = 5; b.touch_note = "";
    return b;
}

// 0 OWN: unique ID 0x8181B3, backlight 21 (the fallback)
// 1 R:   ST7789 0x858552, backlight 27, XPT2046 on the screen bus
// 2 C:   ST7789 0x858552, backlight 27, GT911 at 0x5D (interrupt output on GPIO21)
// 3 BIG: ST7796 via 0xD3 = 0x007796, backlight 27
// 4 NT:  ST7789 0x858552, backlight 21, no touch
static BoardDef B[5];
static uint64_t CONF[MAX_BOARDS][MAX_BOARDS];
static const PanelIds NONE{0, 0xFFFFFF, 0};
static const Probes NOPROBE{};

static void make() {
    B[0] = fixture("OWN", 0x8181B3, NO_ID, 21, TouchType::XPT2046Own);
    B[1] = fixture("R", 0x858552, NO_ID, 27, TouchType::XPT2046Shared);
    B[2] = fixture("C", 0x858552, NO_ID, 27, TouchType::GT911, 0x5D);
    B[3] = fixture("BIG", NO_ID, 0x007796, 27, TouchType::XPT2046Shared);
    B[4] = fixture("NT", 0x858552, NO_ID, 21, TouchType::None);
    memset(CONF, 0, sizeof(CONF));
    CONF[2][0] = 1ull << 21;  // OWN chosen on C: OWN's backlight drives GPIO21 against the GT911 interrupt
    CONF[2][4] = 1ull << 21;
}

static Selection sel(const PanelIds& ids, const char* setting, bool upgrade = false, const Probes& p = NOPROBE) {
    return select(B, 5, CONF, ids, p, setting, upgrade, 0);
}

void test_auto_unique_id_detected() {
    Selection s = sel({0x8181B3, 0xFFFFFF, 0}, "auto");
    TEST_ASSERT_EQUAL(0, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
    TEST_ASSERT_TRUE(s.ask_confirm);
    TEST_ASSERT_TRUE(s.backlight_on && s.touch_on && s.sensors_on);
    TEST_ASSERT_EQUAL_UINT64(0, s.pins_off);
}

void test_auto_rdid4_detects_big_panel() {
    Selection s = sel({0, 0x007796, 0}, "auto");
    TEST_ASSERT_EQUAL(3, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
}

void test_auto_ambiguous_fresh_safe() {
    Selection s = sel({0x858552, 0, 0}, "auto");
    TEST_ASSERT_EQUAL((int)Decision::Ambiguous, (int)s.decision);
    TEST_ASSERT_EQUAL(3, s.n_candidates);  // R, C, NT
    TEST_ASSERT_FALSE(s.backlight_on);    // 27 vs 21: the candidates disagree
    TEST_ASSERT_FALSE(s.sensors_on);
    TEST_ASSERT_FALSE(s.touch_on);
    TEST_ASSERT_FALSE(s.ask_confirm);
}

void test_auto_ambiguous_same_bl_keeps_backlight() {
    B[4].bl_pin = 27;  // now R, C and NT all use 27
    Selection s = sel({0x858552, 0, 0}, "auto");
    TEST_ASSERT_EQUAL((int)Decision::Ambiguous, (int)s.decision);
    TEST_ASSERT_TRUE(s.backlight_on);
    TEST_ASSERT_FALSE(s.touch_on);
    make();
}

void test_probe_resolves_ambiguity() {
    Probes p; p.i2c_addr = 0x5D; p.xpt_shared = 0;
    Selection s = sel({0x858552, 0, 0}, "auto", false, p);
    TEST_ASSERT_EQUAL(2, s.board);  // C: the GT911 answered
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
    Probes q; q.i2c_addr = 0; q.xpt_shared = 1;
    TEST_ASSERT_EQUAL(1, sel({0x858552, 0, 0}, "auto", false, q).board);  // R
}

void test_probe_that_rules_out_everything_is_ignored() {
    Probes p; p.i2c_addr = 0x14;  // nothing in the list uses 0x14
    Selection s = sel({0x858552, 0, 0}, "auto", false, p);
    TEST_ASSERT_EQUAL(3, s.n_candidates);
}

void test_upgrade_unknown_id_keeps_28() {
    Selection s = sel(NONE, "auto", true);
    TEST_ASSERT_EQUAL(0, s.board);  // fallback: the screen as v0.3.1 drove it
    TEST_ASSERT_EQUAL((int)Decision::Upgrade, (int)s.decision);
    TEST_ASSERT_TRUE(s.backlight_on && s.touch_on && s.sensors_on);
    TEST_ASSERT_TRUE(s.ask_confirm);
    TEST_ASSERT_EQUAL_UINT64(0, s.pins_off);
}

void test_upgrade_with_certain_result_detects() {
    Selection s = sel({0, 0x007796, 0}, "auto", true);  // the reporter's 3.5" board
    TEST_ASSERT_EQUAL(3, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
}

void test_fresh_unknown_safe() {
    Selection s = sel({0x123456, 0, 0}, "auto");
    TEST_ASSERT_EQUAL((int)Decision::Unknown, (int)s.decision);
    TEST_ASSERT_FALSE(s.backlight_on);  // the listed boards disagree on the backlight pin
    TEST_ASSERT_FALSE(s.touch_on || s.sensors_on);
}

void test_unreadable_never_mismatch() {
    Selection s = sel(NONE, "BIG");
    TEST_ASSERT_EQUAL(3, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Chosen, (int)s.decision);
    TEST_ASSERT_FALSE(s.warn_mismatch);
}

void test_choice_matches() {
    Selection s = sel({0x858552, 0, 0}, "R");
    TEST_ASSERT_EQUAL(1, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Chosen, (int)s.decision);
    TEST_ASSERT_FALSE(s.ask_confirm);
}

void test_choice_contradicts_warns() {
    Selection s = sel({0x858552, 0, 0}, "OWN");  // OWN's ID is 0x8181B3
    TEST_ASSERT_EQUAL(0, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Mismatch, (int)s.decision);
    TEST_ASSERT_TRUE(s.warn_mismatch);
    TEST_ASSERT_EQUAL_UINT64(1ull << 21, s.pins_off);  // C is a candidate: its interrupt is on 21
    TEST_ASSERT_FALSE(s.backlight_on);                  // OWN's backlight is GPIO21
}

void test_choice_anyway_no_warning() {
    Selection s = sel({0x858552, 0, 0}, "OWN!");
    TEST_ASSERT_EQUAL((int)Decision::ChosenAnyway, (int)s.decision);
    TEST_ASSERT_FALSE(s.warn_mismatch);
    TEST_ASSERT_EQUAL_UINT64(1ull << 21, s.pins_off);
}

void test_unknown_setting_id_falls_back_to_auto() {
    Selection s = sel({0x8181B3, 0, 0}, "GONE!");
    TEST_ASSERT_TRUE(s.setting_unknown);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
    TEST_ASSERT_EQUAL(0, s.board);
}

void test_no_touch_never_asks_on_screen() {
    B[4].rddid = 0x777777;  // make NT unique
    Selection s = sel({0x777777, 0, 0}, "auto");
    TEST_ASSERT_EQUAL(4, s.board);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
    TEST_ASSERT_FALSE(s.ask_confirm);
    TEST_ASSERT_FALSE(s.touch_on);
    make();
}

void test_incomplete_board_not_selectable() {
    B[0].complete = false;
    Selection s = sel({0x8181B3, 0, 0}, "auto");
    TEST_ASSERT_EQUAL((int)Decision::Unknown, (int)s.decision);
    Selection t = sel({0x8181B3, 0, 0}, "OWN");
    TEST_ASSERT_TRUE(t.setting_unknown);
    make();
}

void test_readable() {
    TEST_ASSERT_FALSE(readable({0, 0, 0}));
    TEST_ASSERT_FALSE(readable({0xFFFFFF, 0xFFFFFF, 0xFFFFFF}));
    TEST_ASSERT_TRUE(readable({0x8181B3, 0xFFFFFF, 0}));
}

void test_touch_offset_maps_into_ui() {
    BoardDef big = B[3];
    big.width = 480; big.height = 320; big.ui_x = 80; big.ui_y = 40;
    int ux, uy;
    TEST_ASSERT_TRUE(map_touch(big, 240, 160, &ux, &uy));
    TEST_ASSERT_EQUAL(160, ux); TEST_ASSERT_EQUAL(120, uy);
    TEST_ASSERT_FALSE(map_touch(big, 10, 10, &ux, &uy));  // in the border
    TEST_ASSERT_TRUE(map_touch(B[0], 319, 239, &ux, &uy));
    TEST_ASSERT_EQUAL(319, ux); TEST_ASSERT_EQUAL(239, uy);
}

// The real list (boards/boards.toml): the owner's board is recognised by its measured ID alone.
void test_real_list_owner_id_unique() {
    Selection s = select(BOARDS, BOARD_COUNT, BOARD_CONFLICT, {0x8181B3, 0xFFFFFF, 0xFFFFFF}, NOPROBE, "auto",
                         false, BOARD_FALLBACK);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)s.decision);
    TEST_ASSERT_EQUAL_STRING("2432S028R-2USB", BOARDS[s.board].id);
}

void test_real_list_upgrade_keeps_owner_board() {
    Selection s = select(BOARDS, BOARD_COUNT, BOARD_CONFLICT, {0, 0, 0}, NOPROBE, "auto", true, BOARD_FALLBACK);
    TEST_ASSERT_EQUAL((int)Decision::Upgrade, (int)s.decision);
    TEST_ASSERT_EQUAL_STRING("2432S028R-2USB", BOARDS[s.board].id);
}

void test_real_list_ili9341_ambiguous_until_probed() {
    PanelIds ili{0, 0xFFFFFF, 0x009341};
    Selection s = select(BOARDS, BOARD_COUNT, BOARD_CONFLICT, ili, NOPROBE, "auto", false, BOARD_FALLBACK);
    TEST_ASSERT_EQUAL((int)Decision::Ambiguous, (int)s.decision);  // 2.8" one-USB, 2.4" R, 2.4" C
    Probes p; p.i2c_addr = 0; p.xpt_shared = 0; p.xpt_own = 1;
    Selection t = select(BOARDS, BOARD_COUNT, BOARD_CONFLICT, ili, p, "auto", false, BOARD_FALLBACK);
    TEST_ASSERT_EQUAL((int)Decision::Detected, (int)t.decision);
    TEST_ASSERT_EQUAL_STRING("2432S028R", BOARDS[t.board].id);
}

void test_qr_url_exact_and_short() {
    char buf[200];
    Probes p; p.i2c_addr = 0; p.xpt_own = 1;
    size_t n = report_qr_url(buf, sizeof(buf), "v0.3.2", "2432S028R-2USB", {0x8181B3, 0xFFFFFF, 0},
                             Decision::ChosenAnyway, p);
    TEST_ASSERT_EQUAL_STRING("https://github.com/bmp/QRPickle/issues/new?template=board_report.yml&fw=v0.3.2"
                             "&board=2432S028R-2USB&ids=8181B3-FFFFFF-000000&decision=anyway&touch=0.1.-1", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
    TEST_ASSERT_TRUE(n <= 170);  // QR version 9 (medium ECC, 180 B): 53 dots, 3 px each on 180 px
    // Longest board id and fw, worst probes: still within 170.
    n = report_qr_url(buf, sizeof(buf), "v0.3.12", "2432S028R-ST7789", {0xFFFFFF, 0xFFFFFF, 0xFFFFFF},
                      Decision::Ambiguous, Probes{});
    TEST_ASSERT_TRUE(n > 0 && n <= 170);
    TEST_ASSERT_EQUAL(0, report_qr_url(buf, 40, "v0.3.2", "x", {0, 0, 0}, Decision::Unknown, p));  // too small
}

int main(int, char**) {
    make();
    UNITY_BEGIN();
    RUN_TEST(test_auto_unique_id_detected);
    RUN_TEST(test_auto_rdid4_detects_big_panel);
    RUN_TEST(test_auto_ambiguous_fresh_safe);
    RUN_TEST(test_auto_ambiguous_same_bl_keeps_backlight);
    RUN_TEST(test_probe_resolves_ambiguity);
    RUN_TEST(test_probe_that_rules_out_everything_is_ignored);
    RUN_TEST(test_upgrade_unknown_id_keeps_28);
    RUN_TEST(test_upgrade_with_certain_result_detects);
    RUN_TEST(test_fresh_unknown_safe);
    RUN_TEST(test_unreadable_never_mismatch);
    RUN_TEST(test_choice_matches);
    RUN_TEST(test_choice_contradicts_warns);
    RUN_TEST(test_choice_anyway_no_warning);
    RUN_TEST(test_unknown_setting_id_falls_back_to_auto);
    RUN_TEST(test_no_touch_never_asks_on_screen);
    RUN_TEST(test_incomplete_board_not_selectable);
    RUN_TEST(test_readable);
    RUN_TEST(test_touch_offset_maps_into_ui);
    RUN_TEST(test_real_list_owner_id_unique);
    RUN_TEST(test_real_list_upgrade_keeps_owner_board);
    RUN_TEST(test_real_list_ili9341_ambiguous_until_probed);
    RUN_TEST(test_qr_url_exact_and_short);
    return UNITY_END();
}
