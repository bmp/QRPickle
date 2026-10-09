# PlatformIO post-build guard: static DRAM headroom and app-slot (flash) budget.
# Fails the build when static DRAM headroom < 4 KB or the image exceeds 95% of the
# OTA app slot; warns above 85%. (Review finding 6.5)
Import("env")
import os
import subprocess

DRAM_SEG_BYTES = 124580       # dram0_0_seg length (ESP32 Arduino 2.x linker script)
DRAM_MIN_HEADROOM = 4096
APP_SLOT_BYTES = 0x1C0000     # app0/app1 in partitions.csv
FLASH_WARN, FLASH_FAIL = 0.85, 0.95

def check(source, target, env):
    elf = str(target[0])
    size_tool = env.subst("$SIZETOOL")
    out = subprocess.run([size_tool, "-A", elf], capture_output=True, text=True).stdout
    sec = {p[0]: int(p[1]) for p in (line.split() for line in out.splitlines()) if len(p) >= 2 and p[1].isdigit()}
    dram = sec.get(".dram0.data", 0) + sec.get(".dram0.bss", 0) + sec.get(".noinit", 0)
    headroom = DRAM_SEG_BYTES - dram
    print(f"[check_size] static DRAM {dram:,} B used, headroom {headroom:,} B (min {DRAM_MIN_HEADROOM:,})")
    if headroom < DRAM_MIN_HEADROOM:
        env.Exit(f"[check_size] FAIL: static DRAM headroom below {DRAM_MIN_HEADROOM} B. "
                 "Move large static buffers to the heap (see tools/dram_report.py).")

def check_bin(source, target, env):
    size = os.path.getsize(str(target[0]))
    ratio = size / APP_SLOT_BYTES
    print(f"[check_size] firmware.bin {size:,} B = {ratio:.1%} of the {APP_SLOT_BYTES:,} B app slot")
    if ratio > FLASH_FAIL:
        env.Exit(f"[check_size] FAIL: image above {FLASH_FAIL:.0%} of the OTA slot")
    if ratio > FLASH_WARN:
        print(f"[check_size] WARN: image above {FLASH_WARN:.0%} of the OTA slot")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", check)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_bin)
