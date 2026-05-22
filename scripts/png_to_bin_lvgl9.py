#!/usr/bin/env python3
import os
import re
import argparse
from PIL import Image

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

    pixels = list(img.getdata())

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
        print(f"✅ Converted: {out_name} (LVGL v9 Split-Map RGB565A8)")
        return True
    return False

def main():
    parser = argparse.ArgumentParser(description="Convert PNGs to LVGL v9 Split-Map .bin")
    parser.add_argument("-f", "--file", help="Filename of the PNG (e.g., icon_web_20x20.png)", required=False, type=str)
    parser.add_argument("-a", "--all", help="Batch convert all PNG files in the assets/img directory", action="store_true")
    args = parser.parse_args()

    # Define base paths
    base_dir = os.path.join(os.path.dirname(__file__), '..')
    src_dir = os.path.join(base_dir, 'assets', 'img')
    out_dir = os.path.join(base_dir, 'data', 'img')

    os.makedirs(out_dir, exist_ok=True)

    if args.all:
        print(f"Scanning {src_dir} for assets...")
        count = 0
        if os.path.exists(src_dir):
            for filename in os.listdir(src_dir):
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

        process_file(input_path, out_dir)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
