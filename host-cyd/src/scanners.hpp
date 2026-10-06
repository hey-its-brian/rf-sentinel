#pragma once
#include <Arduino.h>

// The CYD's own radios: a passive-ish Wi-Fi scan and a BLE scan.
// Both run in the background and publish into these structs.
struct LocalWifi {
  uint16_t total = 0, strongestRssi = 0; int8_t strongest = -127;
  uint32_t lastScanMs = 0; bool scanning = false;
  struct { char ssid[33]; uint8_t ch; int8_t rssi; } top[8]; uint8_t topN = 0;
};
struct LocalBle {
  uint16_t seen = 0;       // unique addresses in the last window
  int8_t strongest = -127;
  uint32_t lastWindowMs = 0;
  struct { char addr[18]; char name[20]; int8_t rssi; } top[8]; uint8_t topN = 0;
};
extern LocalWifi lwifi;
extern LocalBle lble;

void scannersBegin();
void scannersPoll();
