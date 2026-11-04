#!/usr/bin/env python3
"""Convert PNGs to LVGL 9 images (.bin, RGB565A8) for LittleFS (data/img/).

The size comes from the file name (icon_web_20x20.png -> 20 x 20), or with --fit from a box the
image is scaled into, keeping its shape (club_logo.png --fit 60x60 -> club_logo_60x45.bin).
Every image is stored uncompressed: 12-byte header + 3 bytes per pixel. LittleFS stores files in
4 KB blocks, so a file just over a block takes two: each conversion reports the blocks and warns
when a smaller size would fit one block (36 x 36 is the largest square that does).

    python3 scripts/png_to_bin_lvgl9.py --all
    python3 scripts/png_to_bin_lvgl9.py -f 01d_36x36.png
    python3 scripts/png_to_bin_lvgl9.py -f ~/club_logo.png --fit 60x60 --out-dir /tmp
    python3 scripts/png_to_bin_lvgl9.py --report        # blocks used by data/img/*.bin
"""
import os
import re
import math
import argparse
from PIL import Image

HEADER_BYTES = 12
LFS_BLOCK = 4096  # LittleFS block size on the CYD (448 KB partition = 112 blocks)


def bin_size(w, h):
    return HEADER_BYTES + w * h * 3


def blocks(size):
    return math.ceil(size / LFS_BLOCK)


def largest_square_in_blocks(n):
    """Side of the largest square image that fits n LittleFS blocks."""
    return math.isqrt((n * LFS_BLOCK - HEADER_BYTES) // 3)


def fit_box(src_w, src_h, box_w, box_h):
    """The largest size within box_w x box_h with the source's shape (at least 1 x 1)."""
    scale = min(box_w / src_w, box_h / src_h)
    return max(1, round(src_w * scale)), max(1, round(src_h * scale))


def block_note(w, h):
    size = bin_size(w, h)
    n = blocks(size)
    note = f"{size:,} B = {n} LittleFS block{'s' if n > 1 else ''}"
    side = largest_square_in_blocks(n - 1) if n > 1 else None
    if side and size - (n - 1) * LFS_BLOCK < LFS_BLOCK // 4:  # just over: a little smaller saves a block
        note += f" (WARNING: just over {n - 1}; e.g. {side}x{side} fits {n - 1})"
    return note

def get_dimensions(filename):
    match = re.search(r'_(\d+)x(\d+)\.png$', filename, re.IGNORECASE)
    if match:
        return int(match.group(1)), int(match.group(2))
    return None, None

def convert_to_lvgl9_split_bin(input_path, output_path, w, h):
    try:
        img = Image.open(input_path).convert('RGBA')
        img = img.resize((w, h), Image.Resampling.LANCZOS)
    except Exception as e:
        print(f"❌ Error loading {input_path}: {e}")
        return False

    raw = img.tobytes()  # RGBA, 4 bytes per pixel
    pixels = (raw[i:i + 4] for i in range(0, len(raw), 4))

    # LVGL v9 12-Byte Header
    magic = 0x19
    cf = 0x14 # LV_COLOR_FORMAT_RGB565A8
    flags = 0x0000
    stride = w * 2 # Stride of the base RGB565 map is width * 2 bytes

    lv_header = bytearray([
        magic,
        cf,
        flags & 0xFF, (flags >> 8) & 0xFF,
        w & 0xFF, (w >> 8) & 0xFF,
        h & 0xFF, (h >> 8) & 0xFF,
        stride & 0xFF, (stride >> 8) & 0xFF,
        0x00, 0x00
    ])

    rgb_data = bytearray()
    alpha_data = bytearray()

    for r, g, b, a in pixels:
        # Convert to RGB565
        color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        # Block 1: RGB Data (Little Endian)
        rgb_data.append(color565 & 0xFF)
        rgb_data.append((color565 >> 8) & 0xFF)
        # Block 2: Alpha Mask Data
        alpha_data.append(a)

    # LVGL v9 Requires the complete RGB array first, followed by the complete Alpha array
    bin_data = rgb_data + alpha_data

    try:
        with open(output_path, 'wb') as f:
            f.write(lv_header)
            f.write(bin_data)
        return True
    except Exception as e:
        print(f"❌ Error writing {output_path}: {e}")
        return False

def process_file(in_path, out_dir):
    filename = os.path.basename(in_path)
    w, h = get_dimensions(filename)
    if w is None or h is None:
        print(f"❌ Error: Filename '{filename}' must contain dimensions (e.g., _20x20.png)")
        return False

    out_name = filename.replace('.png', '.bin')
    out_path = os.path.join(out_dir, out_name)

    if convert_to_lvgl9_split_bin(in_path, out_path, w, h):
        print(f"✅ Converted: {out_name} (LVGL v9 Split-Map RGB565A8), {block_note(w, h)}")
        return True
    return False


def process_fit(in_path, out_dir, box):
    """Scale into a box (e.g. "60x60"), keeping the shape; the output name gets the real size."""
    m = re.fullmatch(r'(\d+)x(\d+)', box)
    if not m:
        print(f"❌ Error: --fit wants WxH, e.g. 60x60 (got '{box}')")
        return False
    with Image.open(in_path) as img:
        w, h = fit_box(img.width, img.height, int(m.group(1)), int(m.group(2)))
    stem = re.sub(r'_\d+x\d+$', '', os.path.splitext(os.path.basename(in_path))[0])
    out_name = f"{stem}_{w}x{h}.bin"
    if convert_to_lvgl9_split_bin(in_path, os.path.join(out_dir, out_name), w, h):
        print(f"✅ Converted: {out_name}, {block_note(w, h)}")
        return True
    return False


def report(out_dir):
    total = 0
    for name in sorted(os.listdir(out_dir)):
        if not name.endswith('.bin'):
            continue
        size = os.path.getsize(os.path.join(out_dir, name))
        total += blocks(size)
        m = re.search(r'_(\d+)x(\d+)\.bin$', name)
        print(f"{name}: " + (block_note(int(m.group(1)), int(m.group(2))) if m else f"{size:,} B"))
    print(f"Total: {total} blocks = {total * LFS_BLOCK // 1024} KB of LittleFS")

def main():
    parser = argparse.ArgumentParser(description="Convert PNGs to LVGL v9 Split-Map .bin")
    parser.add_argument("-f", "--file", help="Filename of the PNG (e.g., icon_web_20x20.png)", required=False, type=str)
    parser.add_argument("-a", "--all", action="store_true",
                        help="Batch convert all PNG files in the assets/img directory")
    parser.add_argument("--fit", metavar="WxH", help="scale into this box, keeping the shape (logos)")
    parser.add_argument("--out-dir", help="where to write the .bin (default data/img)")
    parser.add_argument("--report", action="store_true", help="LittleFS blocks used by each image in data/img")
    args = parser.parse_args()

    # Define base paths
    base_dir = os.path.join(os.path.dirname(__file__), '..')
    src_dir = os.path.join(base_dir, 'assets', 'img')
    out_dir = args.out_dir or os.path.join(base_dir, 'data', 'img')

    os.makedirs(out_dir, exist_ok=True)

    if args.report:
        report(out_dir)
    elif args.all:
        print(f"Scanning {src_dir} for assets...")
        count = 0
        if os.path.exists(src_dir):
            c_dir = os.path.join(base_dir, 'src', 'ui', 'img')  # built into the firmware (img_to_c.py)
            for filename in sorted(os.listdir(src_dir)):
                if os.path.exists(os.path.join(c_dir, os.path.splitext(filename)[0] + '.c')):
                    continue
                if filename.lower().endswith('.png'):
                    if process_file(os.path.join(src_dir, filename), out_dir):
                        count += 1
            print(f"\n🎯 Total assets converted: {count}")
        else:
            print(f"❌ Error: Source directory '{src_dir}' does not exist.")
    elif args.file:
        # Allow full paths to be passed, but default to checking assets/img/ if just a filename is provided
        if os.path.isfile(args.file):
            input_path = args.file
        else:
            input_path = os.path.join(src_dir, args.file)

        if not os.path.isfile(input_path):
            print(f"❌ Error: Could not find file at '{input_path}'")
            return

        if args.fit:
            process_fit(input_path, out_dir, args.fit)
        else:
            process_file(input_path, out_dir)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
