# RF Sentinel

Receive-only RF and GNSS monitoring node on an M5Stamp C5 (ESP32-C5), plus a host UI for the 2.8" CYD. Listens on 433 MHz (CC1101), 2.4 GHz (nRF24L01+), optionally LoRa 915 (SX1262), reads a GPS, and uses the C5's own dual-band Wi-Fi for presence scanning. Learns what "normal" looks like and raises events when something changes.

No transmit path exists in this firmware. `components/radios/radios.cpp` has a compile-time guard that breaks the build if one is added, and the nRF24 auto-ack is forced off so the chip never answers on its own.

## Layout

```
rf-sentinel/            ESP-IDF 5.5 project for the Stamp C5 (this directory)
  main/                 app loop, host protocol, baseline/alert logic
  components/radios     CC1101 + nRF24 + SX1262 via RadioLib (ESP-IDF HAL)
  components/gps        NMEA parser on UART0
  components/wifiscan   passive dual-band Wi-Fi scan
  docs/protocol.md      frame format
  docs/wiring.md        pin map and power
host-cyd/     PlatformIO (Arduino) host firmware for the ESP32-2432S028R USB-C CYD
```

## Build the node

```sh
. ~/esp/esp-idf-v5.5/export.sh
cd ~/development/my_dev/rf-sentinel
idf.py set-target esp32c5
idf.py menuconfig        # "RF Sentinel pins" if your wiring differs from docs/wiring.md
idf.py build flash monitor
```

The first build downloads RadioLib from the component registry into `managed_components/`.

Watching the USB console you should see `$RFS,HELLO,...` followed by `STAT`, `GPS`, `SUB` and `NRF` rows. A radio that did not answer on SPI shows `cc=0` or `nrf=0` in `HELLO` and a warning in the boot log.

## Build the CYD host

```sh
cd ~/development/my_dev/rf-sentinel/host-cyd
pio run -t upload -t monitor
```

## Release

```sh
tools/release.sh 0.2.0 notes.md   # or write docs/releases/v0.2.0.md first
```

That commits the version bump, tags `v0.2.0` and pushes. The Release workflow builds both images and publishes the GitHub release; the Web flasher workflow then runs from main and redeploys the portal. Pages is enabled by the workflow itself.

## Roadmap

See [ROADMAP.md](ROADMAP.md).

## Status

- [x] Phase 1: bench bring-up firmware (this)
- [x] Phase 2: framed protocol, CYD host UI
- [x] Phase 3: baseline and alerts (first pass)
- [x] Phase 4: GNSS interference watch (first pass: sats used, mean C/N0)
- [x] Phase 5: Wi-Fi presence counts (first pass)
- [ ] DECK//OS `deck_rf` component for the Tab5
- [ ] RFID reader
- [ ] Perfboard / PCB
