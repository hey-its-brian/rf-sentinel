#include "proto.hpp"
#include "model.hpp"
#include "board.h"

static HardwareSerial& node = Serial2;
static char line[640];
static size_t len = 0;

static uint8_t xorSum(const char* p, size_t n) { uint8_t c = 0; while (n--) c ^= (uint8_t)*p++; return c; }

void protoBegin() {
  node.begin(NODE_BAUD, SERIAL_8N1, PIN_NODE_RX, PIN_NODE_TX);
  node.setRxBufferSize(2048);
}

void protoSend(const char* payload) {
  char body[96];
  snprintf(body, sizeof body, "RFS,%s", payload);
  node.printf("$%s*%02X\n", body, xorSum(body, strlen(body)));
}

// Split on commas in place. Returns field count.
static int split(char* s, char* f[], int max) {
  int n = 0; char* p = s;
  while (n < max && p) { f[n++] = p; p = strchr(p, ','); if (p) *p++ = 0; }
  return n;
}

static void handle(char* s) {
  if (s[0] != '$') return;
  char* star = strchr(s, '*');
  if (star) {
    unsigned want = strtoul(star + 1, nullptr, 16);
    if (xorSum(s + 1, star - s - 1) != (uint8_t)want) return;
    *star = 0;
  }
  if (strncmp(s, "$RFS,", 5) != 0) return;
  char* f[32]; int n = split(s + 5, f, 32);
  if (n < 1) return;
  Model& m = model;
  m.nodeSeen = true; m.lastFrameMs = millis();
  const char* t = f[0];

  if (!strcmp(t, "HELLO") && n >= 8) {
    strlcpy(m.fw, f[1], sizeof m.fw);
    m.ccOk = f[3][3] == '1'; m.nrfOk = f[4][4] == '1'; m.loraOk = f[5][5] == '1'; m.wifiOk = f[7][5] == '1';
    m.pushEvent(false, "node", "node online");
  } else if (!strcmp(t, "STAT") && n >= 7) {
    m.uptime = strtoul(f[1], 0, 10);
    m.ccOk = f[2][0] == '1'; m.nrfOk = f[3][0] == '1'; m.loraOk = f[4][0] == '1'; m.gpsOk = f[5][0] == '1';
    m.freeHeap = strtoul(f[6], 0, 10);
  } else if (!strcmp(t, "GPS") && n >= 11) {
    m.fix = atoi(f[1]); m.satsUsed = atoi(f[2]); m.satsView = atoi(f[3]); m.hdop = atof(f[4]);
    m.lat = atof(f[5]); m.lon = atof(f[6]); m.alt = atof(f[7]); m.cn0Avg = atof(f[8]); m.cn0Max = atoi(f[9]);
    strlcpy(m.utc, f[10], sizeof m.utc);
  } else if (!strcmp(t, "SUB") && n >= 2 + SUB_CH) {
    for (int i = 0; i < SUB_CH; i++) {
      m.sub[i] = atoi(f[2 + i]);
      if (m.sub[i] > m.subPeak[i]) m.subPeak[i] = m.sub[i];
    }
  } else if (!strcmp(t, "NRF") && n >= 3) {
    const char* h = f[2];
    for (int i = 0; i < NRF_CH && h[i]; i++) {
      char c = h[i];
      m.nrf[i] = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0;
      if (m.nrf[i] > m.nrfPeak[i]) m.nrfPeak[i] = m.nrf[i];
    }
  } else if (!strcmp(t, "LORA") && n >= 4) {
    m.loraPkts += atoi(f[1]); m.loraRssi = atof(f[2]); m.loraSnr = atof(f[3]);
  } else if (!strcmp(t, "WIFI") && n >= 5) {
    m.wTotal = atoi(f[1]); m.w24 = atoi(f[2]); m.w5 = atoi(f[3]); m.wStrongest = atoi(f[4]);
    m.wapFill = 0;
  } else if (!strcmp(t, "WAP") && n >= 5) {
    if (m.wapFill < WAP_MAX) {
      auto& w = m.wap[m.wapFill++];
      strlcpy(w.bssid, f[1], sizeof w.bssid); w.ch = atoi(f[2]); w.rssi = atoi(f[3]);
      strlcpy(w.ssid, f[4], sizeof w.ssid);
      m.wapN = m.wapFill;
    }
  } else if (!strcmp(t, "EVT") && n >= 4) {
    // text may itself contain commas: rejoin fields 3..n-1
    static char text[96]; text[0] = 0;
    for (int i = 3; i < n; i++) { if (i > 3) strlcat(text, ",", sizeof text); strlcat(text, f[i], sizeof text); }
    m.pushEvent(!strcmp(f[1], "warn"), f[2], text);
  }
}

void protoPoll() {
  while (node.available()) {
    char c = (char)node.read();
    if (c == '\n' || c == '\r') {
      if (len) { line[len] = 0; handle(line); len = 0; }
    } else if (len < sizeof line - 1) line[len++] = c;
    else len = 0;
  }
}
