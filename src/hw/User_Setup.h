#ifndef USER_SETUP_H
#define USER_SETUP_H

// Identification tag mapping this file layout explicitly to the QRPickle project setup
#define USER_SETUP_INFO "QRPickle"

// The magical clone driver!
// Alternate initialization sequence used for specific CYD display panel clone variants
#define ILI9341_2_DRIVER

#define TFT_RGB_ORDER TFT_BGR

// Display boundary sizes mapping landscape/portrait orientation limits
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Hardware Pins for CYD
#define TFT_MISO 12           // Master In Slave Out: Hardware SPI data receive trace line
#define TFT_MOSI 13           // Master Out Slave In: Hardware SPI data transmit trace line
#define TFT_SCLK 14           // Serial Clock: Hardware SPI bus clock line
#define TFT_CS   15           // Chip Select: Pin to enable/target display panel SPI communications
#define TFT_DC    2           // Data/Command: Hardware register select pin
#define TFT_RST  -1           // Reset: Set to -1 because the physical trace is tied directly to 3.3V
#define TFT_BL   21           // Backlight Control: GPIO pin routing to panel backlight transistor
#define TFT_BACKLIGHT_ON HIGH // Logic level state required to illuminate the backlight array

// Disabled here to prevent TFT_eSPI from claiming control over the secondary touch bus interface
// #define TOUCH_CS 33

// Font Asset Allocation: Loads core raster fonts directly into compilation workspace
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define SMOOTH_FONT           // Compiles support for anti-aliased font processing routines

// Operating Communication Bus Frequencies
#define SPI_FREQUENCY       40000000 // SPI clock frequency for writing graphical data frames (40MHz)
#define SPI_READ_FREQUENCY  20000000 // SPI clock frequency for reading display memory values (20MHz)
// #define SPI_TOUCH_FREQUENCY 2500000

#endif // USER_SETUP_H
