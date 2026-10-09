#!/usr/bin/env python3
"""Report static DRAM usage (dram0_0_seg) from a linker map: headroom and the largest
.data/.bss contributors. Build the map with:
  PLATFORMIO_BUILD_FLAGS='-Wl,-Map,${BUILD_DIR}/firmware.map' pio run
Usage: python3 -I tools/dram_report.py .pio/build/cyd/firmware.map [top_n]
"""
import re
import sys
t = open(sys.argv[1], errors='replace').read()
seg = re.search(r'^dram0_0_seg\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)', t, re.M)
org, ln = int(seg.group(1), 16), int(seg.group(2), 16)
end = int(re.search(r'0x([0-9a-f]+)\s+_heap_start = ', t).group(1), 16)
print(f'dram0_0_seg {ln:,} B, used {end-org:,} B, headroom {org+ln-end:,} B')
body = t[t.find('Linker script and memory map'):]
items, pend = [], None
for line in body.splitlines():
    m = re.match(r'^ (\.(?:dram1|bss|data|sbss|sdata|noinit)\.?\S*)\s*$', line)
    if m: pend = m.group(1); continue
    m = re.match(r'^ (\.(?:dram1|bss|data|sbss|sdata)\.\S+)?\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+)', line)
    if m:
        sym, addr, size = (m.group(1) or pend), int(m.group(2), 16), int(m.group(3), 16)
        if org <= addr < org + ln and size >= 256:
            obj = re.sub(r'.*/', '', m.group(4))
            items.append((size, sym or '?', obj))
        pend = None
for size, sym, obj in sorted(items, reverse=True)[:int(sys.argv[2]) if len(sys.argv) > 2 else 20]:
    print(f'{size:>8,}  {sym[:48]:48}  {obj[:40]}')
