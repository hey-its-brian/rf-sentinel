# RF Sentinel host for the 2.8" CYD

Host UI for the [RF Sentinel](..) node, for the 2.8" ESP32-2432S028R (single USB-C revision). PlatformIO, Arduino core, LovyanGFX, NimBLE.

Pages (tap the bottom bar): HOME (status, mini spectra, event log), 433 (CC1101 sweep), 2.4G (nRF24 carrier map with Wi-Fi ch 1/6/11 markers), GPS (fix, C/N0 bars, interference notes), WIFI (node's dual-band list next to the CYD's own 2.4 GHz list), BLE (CYD's own passive BLE scan).

Wiring is in `../docs/wiring.md`. Four wires on CN1 and P1.

```sh
pio run -t upload -t monitor
```

The onboard RGB LED shows green when the node link is alive and red for ten seconds after a warn event.

If the display is mirrored or colours are off, see the `LCD_INVERT` / `LCD_RGB_ORDER` flags in `include/board.h`; a few USB-C batches shipped with an ST7789 instead of an ILI9341, in which case swap `Panel_ILI9341` for `Panel_ST7789` in `src/lgfx_config.hpp` and set `LCD_INVERT true`.

## Look and feel

The UI follows the DECK//OS theme of the Tab5 cyberdeck (`tab5-cyberdeck/components/deck_ui`): the same palette (background #07070D, panels #0D0F1C with #2A3050 outlines, cyan #00F0FF primary, magenta #FF2A6D secondary, yellow #F3E600 for caution, green #39FF14 for healthy, red #FF3B3B for alarms), chamfered panels with a magenta tab, outlined status chips, an `RF//SENTINEL` brand with accented slashes, `NN // PAGE` titles and `// SECTION` headings. Fonts are LovyanGFX built-ins: `fonts::Orbitron_Light_24` (the same Orbitron family DECK//OS uses for display text) rendered into a small temporary sprite and scaled down with anti-aliasing for the title, headings and nav labels, and `fonts::Font0` (6x8 mono) standing in for Share Tech Mono for all data.
