#pragma once
// ESP32-2432S028R, single USB-C revision ("CYD2USB"). Same pin map as the micro-USB original.

// Display (HSPI)
#define PIN_LCD_SCK   14
#define PIN_LCD_MOSI  13
#define PIN_LCD_MISO  12
#define PIN_LCD_CS    15
#define PIN_LCD_DC    2
#define PIN_LCD_RST   -1
#define PIN_LCD_BL    21
#define LCD_SPI_HZ    40000000
#define LCD_INVERT    false
#define LCD_RGB_ORDER false   // set true if red and blue are swapped

// Touch (XPT2046 on its own bit-banged bus)
#define PIN_TOUCH_SCK  25
#define PIN_TOUCH_MOSI 32
#define PIN_TOUCH_MISO 39
#define PIN_TOUCH_CS   33
#define PIN_TOUCH_IRQ  36

// Onboard extras
#define PIN_LED_R 4
#define PIN_LED_G 16
#define PIN_LED_B 17
#define PIN_LDR   34
#define PIN_SPK   26

// RF Sentinel node link on CN1 (GND, IO22, IO27, 3V3)
#define PIN_NODE_TX 22    // CYD -> C5 G2
#define PIN_NODE_RX 27    // C5 G10 -> CYD
#define NODE_BAUD   115200
