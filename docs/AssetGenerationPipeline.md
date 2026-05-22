# Asset Generation Pipeline

To display clean textual content on the 320x240 screen without vector aliasing artifacts, the system maps anti-aliased font bitmaps pre-rendered from `.ttf` vector files.

## The Conversion Engine Pipeline
Typography assets are generated using the node-backed engine workspace or the standalone offline command wrapper **`lv_font_conv`**. This module compiles vector layout curves into optimized horizontal subpixel bitmasks.

## Compilation Parameters Layout
To introduce or modify custom typography classes, execute the generation call matching these parameters:

```bash
# Generate the dense monospaced diagnostic console font asset parameters
lv_font_conv --font assets/fonts/JetBrainsMono-Bold.ttf -r 0x20-0x7F,0xB0 --size 10 --format lvgl --bpp 4 -o src/ui/fonts/font_jetbrains_10.c

# Generate standard readable structural UI label parameters
lv_font_conv --font assets/fonts/Atkinson-Hyperlegible.ttf -r 0x20-0x7F --size 14 --format lvgl --bpp 4 -o src/ui/fonts/font_atkinson_14.c
```

## Explanatory Flag Matrix
* `--font`: Specifies the incoming TrueType source tracking container path.
* `-r 0x20-0x7F,0xB0`: Sets the ASCII glyph character range restriction (including the degree symbol `°` via `0xB0`). This prevents compiling unnecessary glyph fields, saving massive amounts of flash.
* `--size`: Set height parameters matched in vertical screen pixels.
* `--bpp 4`: Sets the anti-aliasing bit depth level to 4 bits per pixel, creating smooth text elements without generating oversized files.

---

## PlatformIO Deployment Integration Workflow

Once assets are generated using the toolchains, apply these PlatformIO orchestration workflows from your project terminal console to deploy them to the device:

### Python Scripts

A few assets are converted to C files to ensure quick load before LittleFS is loaded, whereas the rest are converted to bin files to help with saving space in the main partition for complex logic. The two files are `img_to_c.py` and `png_to_bin_lvgl9.py` respectively.

### Compile and Flash Software Layout Logic
Compiles the structural source files (`.cpp`) and asset arrays (`.c`), uploading the firmware payload to application flash space:
```bash
pio run -t upload
```

### Compile and Push Filesystem Images
Formats the local `data/` folder directory tree into a flat binary image using `mklittlefs`, uploading it directly to the storage partition:
```bash
pio run -t uploadfs
```

### Launch Unified Audit Monitoring Console
Executes flash injection operations and hooks the system runtime log monitor stream inside a single terminal sequence loop:
```bash
pio run -t upload -t monitor
```
