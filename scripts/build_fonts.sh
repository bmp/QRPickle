#!/bin/bash

# Ensure output directory exists and wipe any old generated files
mkdir -p src/ui/fonts
rm -f src/ui/fonts/*.c

echo "🔍 Checking for local TTF fonts in assets/fonts/..."
if [ ! -f "assets/fonts/AtkinsonHyperlegible-Regular.ttf" ] || [ ! -f "assets/fonts/JetBrainsMono-Bold.ttf" ]; then
    echo "❌ Error: TTF fonts not found!"
    echo "Please ensure 'AtkinsonHyperlegible-Regular.ttf' and 'JetBrainsMono-Bold.ttf' are in the 'assets/fonts/' folder."
    exit 1
fi

echo "⚙️  Compiling Atkinson Hyperlegible (14px)..."
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 14 \
    --font assets/fonts/AtkinsonHyperlegible-Regular.ttf -r 0x20-0x7F \
    --format lvgl -o src/ui/fonts/font_atkinson_14_raw.c

echo "⚙️  Compiling Atkinson Hyperlegible (18px)..."
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 18 \
    --font assets/fonts/AtkinsonHyperlegible-Regular.ttf -r 0x20-0x7F \
    --format lvgl -o src/ui/fonts/font_atkinson_18_raw.c

echo "⚙️  Compiling JetBrains Mono (14px)..."
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 14 \
    --font assets/fonts/JetBrainsMono-Bold.ttf -r 0x20-0x7F \
    --format lvgl -o src/ui/fonts/font_jetbrains_14_raw.c

echo "⚙️  Compiling JetBrains Mono (24px)..."
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 24 \
    --font assets/fonts/JetBrainsMono-Bold.ttf -r 0x20-0x7F \
    --format lvgl -o src/ui/fonts/font_jetbrains_24_raw.c

echo "✅ Fonts successfully built into src/ui/fonts/ with _raw extensions!"
