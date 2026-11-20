#pragma once
// Setup over USB: the line protocol between the web installer page and the board (host-safe;
// test/test_setup_serial). Page -> board, one line each, prefix "QRPSETUP ":
//   HELLO | BEGIN <bytes> | DATA <seq> <text> | APPLY
// The board answers READY, ACK BEGIN, ACK <seq> or ERR length|seq|timeout|command. The JSON
// (at most MAX_JSON bytes, chunks of at most MAX_DATA) is malloc'd at BEGIN and freed by done(),
// an error or the timeout, so the protocol costs no RAM while nobody is setting up.
#include <stddef.h>
#include <stdint.h>

namespace services {
    namespace setup_proto {

        constexpr size_t MAX_JSON = 1024, MAX_DATA = 160;
        constexpr uint32_t TIMEOUT_MS = 10000;  // no line for this long mid-transfer: ERR timeout

        enum class Act : uint8_t { None,
                                   Reply,
                                   Apply };
        struct Result {
            Act act = Act::None;
            char reply[48] = "";
            const char* json = nullptr;  // Apply: the JSON, NUL-terminated; valid until done()
            size_t json_len = 0;
        };

        class Proto {
        public:
            ~Proto();
            Result line(const char* l, uint32_t now_ms);  // one complete line, without "\n"
            Result tick(uint32_t now_ms);  // reports the timeout once
            void done();  // after Apply: frees the JSON

        private:
            char* buf = nullptr;
            size_t want = 0, have = 0;
            uint32_t next_seq = 0, last_ms = 0;
            void reset();
            Result reply(const char* text);
            Result fail(const char* why);
        };

    }  // namespace setup_proto
}  // namespace services
