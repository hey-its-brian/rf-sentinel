#pragma once
#include <Arduino.h>

constexpr int SUB_CH = 21;
constexpr int NRF_CH = 126;
constexpr int EVT_MAX = 8;
constexpr int WAP_MAX = 8;

struct Event { uint32_t ms; bool warn; char src[6]; char text[48]; };

struct Model {
  // link
  bool nodeSeen = false;
  uint32_t lastFrameMs = 0;
  char fw[12] = "?";
  bool ccOk = false, nrfOk = false, loraOk = false, gpsOk = false, wifiOk = false;
  uint32_t uptime = 0, freeHeap = 0;
  // radios
  int8_t sub[SUB_CH];        // dBm
  int8_t subPeak[SUB_CH];    // slow decaying peak for the display
  uint8_t nrf[NRF_CH];       // 0..8 hits
  uint8_t nrfPeak[NRF_CH];
  int loraPkts = 0; float loraRssi = 0, loraSnr = 0;
  // gps
  uint8_t fix = 0, satsUsed = 0, satsView = 0, cn0Max = 0;
  float hdop = 0, alt = 0, cn0Avg = 0;
  double lat = 0, lon = 0;
  char utc[11] = "0";
  // node wifi
  uint16_t wTotal = 0, w24 = 0, w5 = 0; int8_t wStrongest = -127;
  struct { char bssid[13]; uint8_t ch; int8_t rssi; char ssid[33]; } wap[WAP_MAX];
  uint8_t wapN = 0, wapFill = 0;
  // events ring
  Event evt[EVT_MAX]; uint8_t evtHead = 0, evtCount = 0;
  uint32_t warnCount = 0;

  Model() {
    for (int i = 0; i < SUB_CH; i++) sub[i] = subPeak[i] = -120;
    memset(nrf, 0, sizeof nrf); memset(nrfPeak, 0, sizeof nrfPeak);
  }
  void pushEvent(bool warn, const char* src, const char* text) {
    Event& e = evt[evtHead];
    e.ms = millis(); e.warn = warn;
    strlcpy(e.src, src, sizeof e.src);
    strlcpy(e.text, text, sizeof e.text);
    evtHead = (evtHead + 1) % EVT_MAX;
    if (evtCount < EVT_MAX) evtCount++;
    if (warn) warnCount++;
  }
  bool linkAlive() const { return nodeSeen && (millis() - lastFrameMs) < 4000; }
};

extern Model model;
