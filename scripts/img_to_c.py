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

def sanitize_var_name(filename):
    name = os.path.splitext(filename)[0]
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    if name[0].isdigit():
        name = '_' + name
    return name

def convert_to_lvgl9_split_c(input_path, output_path, w, h, var_name):
    try:
        img = Image.open(input_path).convert('RGBA')
        img = img.resize((w, h), Image.Resampling.LANCZOS)
    except Exception as e:
        print(f"❌ Error loading {input_path}: {e}")
        return False

    pixels = list(img.getdata())
    rgb_data = []
    alpha_data = []

    for r, g, b, a in pixels:
        # Convert RGB to 16-bit RGB565 (Little Endian)
        color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        byte1 = color565 & 0xFF
        byte2 = (color565 >> 8) & 0xFF
        
        rgb_data.append(f"0x{byte1:02X}")
        rgb_data.append(f"0x{byte2:02X}")
        alpha_data.append(f"0x{a:02X}")

    # FIXED: Combine arrays sequentially to strictly honor the LVGL v9 Split-Map format
    combined_bytes = rgb_data + alpha_data

    # Format into rows of 12 hex values for clean code generation
    formatted_pixels = ""
    for i in range(0, len(combined_bytes), 12):
        formatted_pixels += "    " + ", ".join(combined_bytes[i:i+12]) + ",\n"

    stride = w * 2

    c_template = f"""#include <lvgl.h>

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

/* Image format: LV_COLOR_FORMAT_RGB565A8 (Split-Map layout for DMA alignment) */
const LV_ATTRIBUTE_MEM_ALIGN uint8_t {var_name}_map[] = {{
{formatted_pixels}}};

const lv_image_dsc_t {var_name} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565A8,
        .flags = 0,
        .w = {w},
        .h = {h},
        .stride = {stride},
    }},
    .data_size = sizeof({var_name}_map),
    .data = {var_name}_map,
    .reserved = NULL
}};
"""

    try:
        with open(output_path, 'w') as f:
            f.write(c_template)
        return True
    except Exception as e:
        print(f"❌ Error writing {output_path}: {e}")
        return False

def process_file(in_path, out_dir):
    filename = os.path.basename(in_path)
    w, h = get_dimensions(filename)
    if w is None or h is None:
        print(f"⚠️ Skipping {filename}: Must have dimensions in name (e.g., logo_splash_left_50x50.png)")
        return False

    var_name = sanitize_var_name(filename)
    out_name = var_name + '.c'
    out_path = os.path.join(out_dir, out_name)

    if convert_to_lvgl9_split_c(in_path, out_path, w, h, var_name):
        print(f"✅ Converted: {out_name} ({w}x{h} LVGL v9 Split-Map C-Array)")
        return True
    return False

def main():
    parser = argparse.ArgumentParser(description="Convert PNGs to LVGL v9 Split-Map C arrays")
    parser.add_argument("-f", "--file", help="Convert a specific PNG file", type=str)
    args = parser.parse_args()

    out_dir = os.path.join(os.path.dirname(__file__), '..', 'src', 'ui', 'img')
    os.makedirs(out_dir, exist_ok=True)

    if args.file:
        process_file(args.file, out_dir)
    else:
        src_dir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'img')
        print(f"Scanning {src_dir} for assets...")
        count = 0
        if os.path.exists(src_dir):
            for filename in os.listdir(src_dir):
                if filename.lower().endswith('.png'):
                    if process_file(os.path.join(src_dir, filename), out_dir):
                        count += 1
            print(f"\n🎯 Total Split-Map C-Arrays generated: {count}")
        else:
            print(f"❌ Source directory not found: {src_dir}")

if __name__ == "__main__":
    main()