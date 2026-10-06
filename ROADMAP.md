# RF Sentinel roadmap

Receive-only stays the rule for everything below. Items are rough order, not promises.

## Next

- [ ] **Threat alerts: trackers, skimmers, cameras.** Classify what the scans already see and raise `EVT,warn,...` rows with a named reason instead of a bare channel number.
  - Trackers: BLE advertisements matching AirTag / Find My (Apple 0x004C, type 0x12), Tile, Samsung SmartTag, Chipolo; alert when the same address or payload follows you across GPS positions or time windows (unlike a neighbour's tag that stays put).
  - Skimmers: BLE devices advertising HC-05 / HC-06 / JDY-31 style names or UUIDs, generic "RNBT" / "HC" prefixes, strong RSSI near a card reader; reuse the fingerprints from the skimmer-detector project.
  - Cameras: Wi-Fi APs and probe sources with camera-vendor OUIs (Wyze, Reolink, Hikvision, Dahua, Arlo, Ring, Amcrest, Blink, EufY, TP-Link Tapo), hidden SSIDs with those OUIs, and 2.4 GHz channel occupancy from Wi-Fi video streams; CC1101 side later for 433 MHz PIR and door sensors.
  - Host side: a THREATS page listing matches with first-seen / last-seen / RSSI trend, red LED and optional speaker chirp on the CYD, and a persistent counter in the status bar.
- [ ] **OTA updates.** Host: ArduinoOTA over Wi-Fi plus a "check for update" that pulls the latest `-ota.bin` from GitHub releases, DECK//OS style (SYSTEM > UPDATE). Node: ESP-IDF OTA from the same release, started by the host over the `$RFS` link or by the node's own Wi-Fi. Release workflow gains `*-ota.bin` assets next to the full images.
- [ ] **Web portal with PIN.** The CYD serves a small page on its own Wi-Fi (or the home network): enter a PIN, then see a live mirror of the screen (framebuffer pushed as PNG or a canvas drawn from the same `$RFS` data stream over WebSocket), the event log, and the settings. Same NETRUNNER styling as the flasher page. PIN set on the SET page and stored in NVS; rate-limited, no PIN means no portal.

## Later

- [ ] Verify the node on hardware (CC1101, nRF24, GPS, Wi-Fi), tune baseline thresholds from real data.
- [ ] DECK//OS `deck_rf` component and RF WATCH module for the Tab5.
- [ ] Cardputer host.
- [ ] SX1262 LoRa listener enabled and tested on 915.
- [ ] GNSS spoofing heuristics (position jumps, clock jumps, C/N0 too uniform).
- [ ] RFID reader.
- [ ] SD logging on the CYD (CSV of events and sweeps).
- [ ] Perfboard, then PCB, then case.

## Done

- [x] v0.1.0: node firmware, CYD host, web flasher, CI.
- [x] v0.2.0: DECK//OS theme, settings page, touch calibration, local Wi-Fi scan fix.
