#include "scanners.hpp"
#include <WiFi.h>
#include <NimBLEDevice.h>

LocalWifi lwifi;
LocalBle lble;

static const uint32_t WIFI_PERIOD_MS = 20000;
static const uint32_t BLE_WINDOW_MS = 5000;

// --- BLE -------------------------------------------------------------------
// Collect unique addresses per window; keep the 8 strongest.
struct Seen { uint8_t addr[6]; int8_t rssi; char name[20]; };
static Seen seen[64]; static uint8_t seenN = 0;
static NimBLEScan* scan;

class AdvCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* d) override {
    const uint8_t* a = d->getAddress().getNative();
    for (int i = 0; i < seenN; i++)
      if (!memcmp(seen[i].addr, a, 6)) { if (d->getRSSI() > seen[i].rssi) seen[i].rssi = d->getRSSI(); return; }
    if (seenN >= 64) return;
    Seen& s = seen[seenN++];
    memcpy(s.addr, a, 6); s.rssi = d->getRSSI();
    strlcpy(s.name, d->haveName() ? d->getName().c_str() : "", sizeof s.name);
  }
};

static void bleWindowDone() {
  lble.seen = seenN; lble.strongest = -127; lble.topN = 0;
  // selection sort the strongest 8
  for (int k = 0; k < 8 && k < seenN; k++) {
    int best = k;
    for (int i = k + 1; i < seenN; i++) if (seen[i].rssi > seen[best].rssi) best = i;
    Seen t = seen[k]; seen[k] = seen[best]; seen[best] = t;
    auto& o = lble.top[lble.topN++];
    const uint8_t* a = seen[k].addr;
    snprintf(o.addr, sizeof o.addr, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
    strlcpy(o.name, seen[k].name, sizeof o.name); o.rssi = seen[k].rssi;
    if (o.rssi > lble.strongest) lble.strongest = o.rssi;
  }
  lble.lastWindowMs = millis();
  seenN = 0;
}

void scannersBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  NimBLEDevice::init("");
  scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvCb(), true);
  scan->setActiveScan(false);      // passive: never send scan requests
  // Wi-Fi and BLE share one radio on the classic ESP32. Keep the BLE duty
  // cycle low (30%) or Wi-Fi scans starve and return nothing.
  scan->setInterval(160);
  scan->setWindow(48);
  scan->setMaxResults(0);          // callbacks only, no internal list
  scan->start(0, nullptr, false);  // continuous
}

void scannersPoll() {
  uint32_t now = millis();
  // BLE window roll-over
  if (now - lble.lastWindowMs >= BLE_WINDOW_MS) bleWindowDone();

  // Wi-Fi scan (async so the UI keeps moving)
  static bool first = true;
  if (!lwifi.scanning && (first ? now > 3000 : now - lwifi.lastScanMs >= WIFI_PERIOD_MS)) {
    first = false;
    // passive, 300 ms per channel: long enough to catch at least one beacon
    int16_t r = WiFi.scanNetworks(true, true, true, 300);
    lwifi.scanning = (r == WIFI_SCAN_RUNNING);
    if (!lwifi.scanning) { Serial.printf("[wifi] scan start failed (%d)\n", r); lwifi.lastScanMs = now; }
  }
  if (lwifi.scanning) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      lwifi.total = n; lwifi.strongest = -127; lwifi.topN = 0;
      for (int i = 0; i < n && lwifi.topN < 8; i++) {   // results arrive sorted by RSSI
        auto& t = lwifi.top[lwifi.topN++];
        strlcpy(t.ssid, WiFi.SSID(i).length() ? WiFi.SSID(i).c_str() : "<hidden>", sizeof t.ssid);
        t.ch = WiFi.channel(i); t.rssi = WiFi.RSSI(i);
        if (t.rssi > lwifi.strongest) lwifi.strongest = t.rssi;
      }
      WiFi.scanDelete();
      lwifi.scanning = false; lwifi.lastScanMs = now;
      Serial.printf("[wifi] %u networks, best %d\n", lwifi.total, lwifi.strongest);
    } else if (n == WIFI_SCAN_FAILED) { Serial.println("[wifi] scan failed"); lwifi.scanning = false; lwifi.lastScanMs = now; }
  }
}
