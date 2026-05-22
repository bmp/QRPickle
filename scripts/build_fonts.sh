#!/bin/bash

# Ensure output directory exists
mkdir -p src/ui/fonts

echo "🔍 Checking for local TTF fonts in assets/fonts/..."
if [ ! -f "assets/fonts/AtkinsonHyperlegible-Regular.ttf" ] || [ ! -f "assets/fonts/JetBrainsMono-Bold.ttf" ]; then
    echo "❌ Error: TTF fonts not found!"
    echo "Please ensure 'AtkinsonHyperlegible-Regular.ttf' and 'JetBrainsMono-Bold.ttf' are in the 'assets/fonts/' folder."
    exit 1
fi

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
        --format lvgl -o "$out_path"
}

# --- Atkinson Hyperlegible Generations ---
generate_font "Atkinson Hyperlegible (10px)" 10 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_10_raw.c"
generate_font "Atkinson Hyperlegible (14px)" 14 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_14_raw.c"
generate_font "Atkinson Hyperlegible (18px)" 18 "assets/fonts/AtkinsonHyperlegible-Regular.ttf" "src/ui/fonts/font_atkinson_18_raw.c"

# --- JetBrains Mono Generations ---
generate_font "JetBrains Mono (10px)" 10 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_10_raw.c"
generate_font "JetBrains Mono (14px)" 14 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_14_raw.c"
generate_font "JetBrains Mono (24px)" 24 "assets/fonts/JetBrainsMono-Bold.ttf" "src/ui/fonts/font_jetbrains_24_raw.c"

echo "✅ Font checking and building process complete!"
