#include "setup_serial_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace services {
    namespace setup_proto {

        namespace {
            constexpr char PREFIX[] = "QRPSETUP ";
            constexpr size_t PREFIX_LEN = sizeof(PREFIX) - 1;

            // "<digits>" followed by end or a space; false otherwise.
            bool number(const char*& p, unsigned long& out) {
                if (*p < '0' || *p > '9') return false;
                char* end = nullptr;
                out = strtoul(p, &end, 10);
                if (*end != '\0' && *end != ' ') return false;
                p = end;
                return true;
            }
        }  // namespace

        Proto::~Proto() { free(buf); }

        void Proto::reset() {
            free(buf);
            buf = nullptr;
            want = have = 0;
            next_seq = 0;
        }

        void Proto::done() { reset(); }

        Result Proto::reply(const char* text) {
            Result r;
            r.act = Act::Reply;
            snprintf(r.reply, sizeof(r.reply), "%s%s", PREFIX, text);
            return r;
        }

        Result Proto::fail(const char* why) {
            reset();
            char t[24];
            snprintf(t, sizeof(t), "ERR %s", why);
            return reply(t);
        }

        Result Proto::tick(uint32_t now_ms) {
            if (buf && now_ms - last_ms > TIMEOUT_MS) return fail("timeout");
            return Result{};
        }

        Result Proto::line(const char* l, uint32_t now_ms) {
            if (!l || strncmp(l, PREFIX, PREFIX_LEN) != 0) return Result{};
            const char* cmd = l + PREFIX_LEN;
            last_ms = now_ms;

            if (strcmp(cmd, "HELLO") == 0) return reply("READY");

            if (strncmp(cmd, "BEGIN ", 6) == 0) {
                const char* p = cmd + 6;
                unsigned long n = 0;
                if (!number(p, n) || *p || n == 0 || n > MAX_JSON) return fail("length");
                reset();
                buf = static_cast<char*>(malloc(n + 1));
                if (!buf) return fail("length");
                want = n;
                return reply("ACK BEGIN");
            }

            if (strncmp(cmd, "DATA ", 5) == 0) {
                const char* p = cmd + 5;
                unsigned long seq = 0;
                if (!buf || !number(p, seq) || seq != next_seq || (*p && *p != ' ')) return fail("seq");
                const char* text = *p ? p + 1 : p;  // everything after "<seq> ", spaces included
                const size_t n = strlen(text);
                if (n > MAX_DATA || have + n > want) return fail("length");
                memcpy(buf + have, text, n);
                have += n;
                char t[16];
                snprintf(t, sizeof(t), "ACK %lu", seq);
                next_seq++;
                return reply(t);
            }

            if (strcmp(cmd, "APPLY") == 0) {
                if (!buf) return fail("seq");
                if (have != want) return fail("length");
                buf[have] = '\0';
                Result r;
                r.act = Act::Apply;
                r.json = buf;
                r.json_len = have;
                return r;
            }

            return reply("ERR command");
        }

    }  // namespace setup_proto
}  // namespace services
