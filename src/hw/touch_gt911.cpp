#include "touch_gt911.h"
#include <Arduino.h>
#include <Wire.h>

namespace hw {
    namespace gt911 {

        namespace {
            constexpr uint16_t REG_STATUS = 0x814E;  // bit 7: data ready; bits 0-3: touch points
            constexpr uint16_t REG_POINT1 = 0x8150;  // x low, x high, y low, y high, size low, size high
            uint8_t addr = 0x5D;
            const BoardDef* board = nullptr;

            bool write_reg(uint16_t reg, const uint8_t* data, size_t n) {
                Wire1.beginTransmission(addr);
                Wire1.write((uint8_t)(reg >> 8));
                Wire1.write((uint8_t)reg);
                for (size_t i = 0; i < n; i++) Wire1.write(data[i]);
                return Wire1.endTransmission() == 0;
            }

            bool read_reg(uint16_t reg, uint8_t* out, size_t n) {
                if (!write_reg(reg, nullptr, 0)) return false;
                if (Wire1.requestFrom(addr, (uint8_t)n) != n) return false;
                for (size_t i = 0; i < n; i++) out[i] = (uint8_t)Wire1.read();
                return true;
            }
        }  // namespace

        bool begin(const BoardDef& b) {
            board = &b;
            if (b.t_rst >= 0 && b.t_irq >= 0) {  // reset with INT low: address 0x5D
                pinMode(b.t_rst, OUTPUT);
                pinMode(b.t_irq, OUTPUT);
                digitalWrite(b.t_irq, LOW);
                digitalWrite(b.t_rst, LOW);
                delay(10);
                digitalWrite(b.t_rst, HIGH);
                delay(10);
                pinMode(b.t_irq, INPUT);  // the chip drives INT from now on
                delay(50);
            }
            Wire1.begin(b.t_sda, b.t_scl, 400000);
            for (uint8_t a : {(uint8_t)0x5D, (uint8_t)0x14}) {
                Wire1.beginTransmission(a);
                if (Wire1.endTransmission() == 0) {
                    addr = a;
                    return true;
                }
            }
            Wire1.end();
            return false;
        }

        bool read(int16_t& px, int16_t& py) {
            uint8_t status = 0;
            if (!board || !read_reg(REG_STATUS, &status, 1) || !(status & 0x80)) return false;
            const uint8_t points = status & 0x0F;
            uint8_t p[4] = {0, 0, 0, 0};
            const bool ok = points > 0 && read_reg(REG_POINT1, p, 4);
            const uint8_t clear = 0;
            write_reg(REG_STATUS, &clear, 1);  // ready for the next report
            if (!ok) return false;
            // The chip reports in the panel's native portrait orientation; turn it like the screen.
            int16_t x = (int16_t)(p[0] | p[1] << 8), y = (int16_t)(p[2] | p[3] << 8);
            if (board->swap_xy) {
                int16_t t = x;
                x = y;
                y = t;
            }
            if (board->mirror_x) x = (int16_t)(board->width - 1 - x);
            if (board->mirror_y) y = (int16_t)(board->height - 1 - y);
            px = x;
            py = y;
            return true;
        }

    }  // namespace gt911
}  // namespace hw
