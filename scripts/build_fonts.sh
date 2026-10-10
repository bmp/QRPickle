#!/bin/bash

# Ensure output directory exists
mkdir -p src/ui/fonts

echo "🔍 Checking for local TTF fonts in assets/fonts/..."
if [ ! -f "assets/fonts/AtkinsonHyperlegible-Regular.ttf" ] || [ ! -f "assets/fonts/JetBrainsMono-Bold.ttf" ]; then
    echo "❌ Error: TTF fonts not found!"
    echo "Please ensure 'AtkinsonHyperlegible-Regular.ttf' and 'JetBrainsMono-Bold.ttf' are in the 'assets/fonts/' folder."
    exit 1
fi

# OFL 1.1 requires the copyright notice and licence with every copy, including converted ones.
licence_header() {
    case "$1" in
        *Atkinson*)  echo "Copyright 2020 Braille Institute of America, Inc.|assets/fonts/OFL-AtkinsonHyperlegible.txt" ;;
        *JetBrains*) echo "Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)|assets/fonts/OFL-JetBrainsMono.txt" ;;
    esac
}

prepend_licence() {
    local font_path="$1"
    local out_path="$2"
    local info copyright licence_file tmp
    info="$(licence_header "$font_path")"
    copyright="${info%%|*}"
    licence_file="${info##*|}"
    tmp="$(mktemp)"
    {
        echo "/*"
        echo " * Converted from $(basename "$font_path") with lv_font_conv (glyphs 0x20-0x7F)."
        echo " * $copyright"
        echo " * Licensed under the SIL Open Font License, Version 1.1: see $licence_file"
        echo " */"
        cat "$out_path"
    } > "$tmp" && mv "$tmp" "$out_path"
}

# Helper function to generate fonts with overwrite protection
generate_font() {
    local name="$1"
    local size="$2"
    local font_path="$3"
    local out_path="$4"

    if [ -f "$out_path" ]; then
        read -p "⚠️  $out_path already exists. Overwrite? (y/n): " choice
        case "$choice" in
            [Yy]* )
                echo "Overwriting $name..."
                ;;
            * )
                echo "⏭️  Skipping $name..."
                return 0
                ;;
        esac
    fi

    echo "⚙️  Compiling $name..."
    lv_font_conv --no-compress --no-prefilter --bpp 4 --size "$size" \
        --font "$font_path" -r 0x20-0x7F \
        --format lvgl -o "$out_path" && prepend_licence "$font_path" "$out_path"
}

# --- Atkinson Hyperlegible Generations ---
generate_font "Atkinson Hyperlegible (10px)" 10 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_10_raw.c"
generate_font "Atkinson Hyperlegible (14px)" 14 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_14_raw.c"
generate_font "Atkinson Hyperlegible (18px)" 18 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_18_raw.c"

# --- JetBrains Mono Generations ---
generate_font "JetBrains Mono (10px)" 10 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_10_raw.c"
generate_font "JetBrains Mono (14px)" 14 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_14_raw.c"
generate_font "JetBrains Mono (24px)" 24 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_24_raw.c"

# --- Icons-only fallback (replaces LVGL's Montserrat 10/14; fonts.cpp) ---
# Font Awesome 5 code points of every LV_SYMBOL_ QRPickle and its LVGL widgets use, plus the degree
# sign and bullet from JetBrains Mono. Add a code point here when code starts using a new symbol.
SYMBOLS="0xf00b,0xf00c,0xf00d,0xf013,0xf015,0xf021,0xf043,0xf053,0xf054,0xf06e,0xf070,0xf078,0xf093,0xf11c,0xf124,0xf1eb,0xf55a,0xf8a2"
generate_symbols() {
    local size="$1" out_path="src/ui/fonts/font_symbols_${1}_raw.c" tmp
    if [ -f "$out_path" ]; then
        read -p "⚠️  $out_path already exists. Overwrite? (y/n): " choice
        case "$choice" in [Yy]*) ;; *) echo "⏭️  Skipping symbols ${size}px..."; return 0 ;; esac
    fi
    echo "⚙️  Compiling symbols (${size}px)..."
    lv_font_conv --no-compress --no-prefilter --bpp 4 --size "$size" \
        --font assets/fonts/FontAwesome5-Solid+Brands+Regular.woff -r "$SYMBOLS" \
        --font assets/fonts/JetBrainsMono-Bold.ttf -r 0xB0,0x2022 \
        --format lvgl --lv-include lvgl.h -o "$out_path" || return 1
    tmp="$(mktemp)"
    {
        echo "/*"
        echo " * Icons-only fallback font (LV_SYMBOL_* used by QRPickle and its LVGL widgets, plus degree sign"
        echo " * and bullet), converted with lv_font_conv by scripts/build_fonts.sh."
        echo " * Font Awesome 5 Free glyphs: Copyright Fonticons, Inc. (https://fontawesome.com)"
        echo " * Degree sign and bullet: Copyright 2020 The JetBrains Mono Project Authors"
        echo " * (https://github.com/JetBrains/JetBrainsMono)"
        echo " * Licensed under the SIL Open Font License, Version 1.1: see assets/fonts/OFL-LVGL-builtin.txt and"
        echo " * assets/fonts/OFL-JetBrainsMono.txt"
        echo " */"
        cat "$out_path"
    } > "$tmp" && mv "$tmp" "$out_path" && chmod 644 "$out_path"
}
generate_symbols 10
generate_symbols 14

echo "✅ Font checking and building process complete!"
