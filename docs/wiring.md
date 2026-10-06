# Wiring: RF Sentinel node + 2.8" CYD host

## Stamp C5 pin map (firmware defaults, `idf.py menuconfig` to change)

Only pad GPIOs are used for the core loadout, so no FPC adapter is needed until you add the LoRa module.

| Signal | C5 pad | Notes |
|---|---|---|
| SPI SCK | G1 | shared by CC1101, nRF24, SX1262 |
| SPI MOSI | G3 | |
| SPI MISO | G4 | |
| nRF24 CSN | G5 | |
| nRF24 CE | G6 | |
| nRF24 IRQ | not wired | polled |
| CC1101 CSN | G8 | |
| CC1101 GDO0 | not wired (optional G7) | RSSI is polled over SPI; G7 is a strapping pin, leave it floating unless needed |
| CC1101 GDO2 | not wired | |
| GPS TX -> C5 | G9 | 9600 baud |
| GPS RX <- C5 | not wired | we never configure the module |
| Host UART, C5 TX | G10 | to CYD IO27 |
| Host UART, C5 RX | G2 | from CYD IO22. G2 is a strapping pin; a UART line idles high which matches its boot default. If the C5 ever fails to boot with the CYD powered, move this to the FPC UART (G12) |
| SX1262 NSS / RESET / BUSY / DIO1 | G23 / G0 / G24 / G26 | FPC only, optional (`RFS_LORA`) |

Avoid: G28 (BOOT), G27, G25 (strapping, FPC), G13/G14 (USB).

The Stamp C5 has no onboard antenna. Fit a 2.4/5 GHz IPEX antenna or Wi-Fi scanning will see nothing.

## Module connections

All radios and the GPS are 3.3 V logic and supply. Do not feed 5 V to any of them.

### CC1101 (Ebyte E07-433M20S)

| Module pin | Goes to |
|---|---|
| VCC | 3V3 rail |
| GND | GND |
| SCK | G1 |
| MOSI | G3 |
| MISO | G4 |
| CSN | G8 |
| GDO0 | (optional) G7 |
| GDO2 | n/c |
| RXEN | 3V3 (tie high permanently) |
| TXEN | GND (tie low permanently) |

Tying RXEN high and TXEN low hard-wires the E07's front end into receive. Even a firmware bug cannot key the PA.

### nRF24L01+ PA+LNA

| Module pin | Goes to |
|---|---|
| VCC | 3V3 rail, with 10 uF plus 100 nF right at the module |
| GND | GND |
| SCK | G1 |
| MOSI | G3 |
| MISO | G4 |
| CSN | G5 |
| CE | G6 |
| IRQ | n/c |

The PA+LNA version is noisy on its supply; without the capacitor it will intermittently fail to answer on SPI.

### GPS (ATGM336H, or GT-U7 as spare)

| Module pin | Goes to |
|---|---|
| VCC | 3V3 rail |
| GND | GND |
| TX | G9 |
| RX | n/c |
| PPS | n/c (future) |

### SX1262 Stamp LoRa (optional, later)

NSS G23, RESET G0, BUSY G24, DIO1 G26, plus shared SPI. All on the FPC.

## Power

Budget on the 3.3 V rail: C5 Wi-Fi peaks ~350 mA, nRF24 PA+LNA ~45 mA receiving, CC1101 ~17 mA, GPS ~35 mA. That is too much for a Stamp's onboard LDO to share, so:

- Feed 5 V from the CYD (P1 connector, `VIN` pin) to the C5's 5 V input pad (check the silkscreen; do not put 5 V on `BAT`).
- Fit a separate 3.3 V regulator rated 500 mA or better (an AMS1117-3.3 is fine on the bench, a 3.3 V buck is nicer in the case) fed from the same 5 V, and run the CC1101, nRF24 and GPS from that.
- Common ground everywhere.

During bench work it is simpler to power the C5 from its own USB-C and only run GND, TX, RX to the CYD. Both boards can be plugged into the Mac at once; just make sure the grounds are joined.

## CYD side (ESP32-2432S028R, single USB-C revision)

The 2.8" CYD breaks out very little, which is why the C5 does the radio work. Connectors:

| Connector | Pins (from the edge) | Used for |
|---|---|---|
| P1 (4-pin JST 1.25) | VIN, TX (IO1), RX (IO3), GND | 5 V out to the node. Leave TX/RX alone, they are the USB console |
| CN1 (4-pin JST 1.25) | GND, IO22, IO27, 3V3 | Host UART: IO22 = CYD TX -> C5 G2, IO27 = CYD RX <- C5 G10 |
| P3 (3-pin) | GND, IO35, IO22, IO21 | IO21 is the backlight, do not use |

So the full cable is four wires: `P1 VIN -> C5 5V`, `P1 GND -> C5 GND`, `CN1 IO22 -> C5 G2`, `CN1 IO27 <- C5 G10`.

On the CYD, IO22 and IO27 are the only free GPIOs that are not input-only or tied to something, so UART2 is remapped to them in `host-cyd/include/board.h`.

## Antennas

- Keep the three screw antennas apart from each other and from the C5's IPEX antenna; the 433 MHz whip is the long one.
- The GPS patch wants sky and distance from the 2.4 GHz antennas.
