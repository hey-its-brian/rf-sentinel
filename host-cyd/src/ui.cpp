#include "ui.hpp"
#include "lgfx_config.hpp"
#include "model.hpp"
#include "scanners.hpp"
#include "proto.hpp"
#include <functional>
#include <initializer_list>

static LGFX lcd;
static Page page = PG_HOME;
static uint32_t lastDraw = 0;
static bool full = true;

// Layout (landscape 320x240)
static const int W = 320, H = 240, TOP = 18, NAV = 26;
static const int BODY_Y = TOP, BODY_H = H - TOP - NAV;

// Palette: dark background, amber/cyan accents, red for warnings
static const uint16_t C_BG = lcd.color565(10, 12, 16);
static const uint16_t C_PANEL = lcd.color565(22, 26, 34);
static const uint16_t C_TXT = lcd.color565(220, 220, 220);
static const uint16_t C_DIM = lcd.color565(120, 125, 135);
static const uint16_t C_ACC = lcd.color565(255, 176, 0);
static const uint16_t C_CYAN = lcd.color565(0, 200, 220);
static const uint16_t C_OK = lcd.color565(70, 200, 90);
static const uint16_t C_WARN = lcd.color565(230, 60, 60);
static const uint16_t C_GRID = lcd.color565(40, 46, 56);

static const char* NAMES[PG_COUNT] = {"HOME", "433", "2.4G", "GPS", "WIFI", "BLE"};

static void clearBody() { lcd.fillRect(0, BODY_Y, W, BODY_H, C_BG); }

static void drawTop() {
  lcd.fillRect(0, 0, W, TOP, C_PANEL);
  lcd.setTextSize(1); lcd.setTextDatum(lgfx::middle_left);
  lcd.setTextColor(C_ACC, C_PANEL);
  lcd.drawString("RF SENTINEL", 4, TOP / 2);
  bool link = model.linkAlive();
  lcd.setTextColor(link ? C_OK : C_WARN, C_PANEL);
  lcd.drawString(link ? "NODE" : "NO NODE", 92, TOP / 2);
  lcd.setTextColor(model.ccOk ? C_OK : C_DIM, C_PANEL); lcd.drawString("CC", 150, TOP / 2);
  lcd.setTextColor(model.nrfOk ? C_OK : C_DIM, C_PANEL); lcd.drawString("NRF", 170, TOP / 2);
  lcd.setTextColor(model.gpsOk ? C_OK : C_DIM, C_PANEL); lcd.drawString("GPS", 196, TOP / 2);
  lcd.setTextColor(model.fix ? C_OK : C_DIM, C_PANEL);
  char b[16]; snprintf(b, sizeof b, "%dsat", model.satsUsed); lcd.drawString(b, 224, TOP / 2);
  lcd.setTextDatum(lgfx::middle_right);
  lcd.setTextColor(model.warnCount ? C_WARN : C_DIM, C_PANEL);
  snprintf(b, sizeof b, "!%lu", (unsigned long)model.warnCount); lcd.drawString(b, W - 4, TOP / 2);
}

static void drawNav() {
  int bw = W / PG_COUNT;
  for (int i = 0; i < PG_COUNT; i++) {
    bool on = i == page;
    lcd.fillRect(i * bw, H - NAV, bw - 1, NAV, on ? C_ACC : C_PANEL);
    lcd.setTextDatum(lgfx::middle_center);
    lcd.setTextColor(on ? C_BG : C_TXT, on ? C_ACC : C_PANEL);
    lcd.drawString(NAMES[i], i * bw + bw / 2, H - NAV / 2);
  }
}

// ---- helpers ---------------------------------------------------------------
static void label(int x, int y, const char* k, const char* v, uint16_t vc = C_TXT) {
  lcd.setTextDatum(lgfx::top_left);
  lcd.setTextColor(C_DIM, C_BG); lcd.drawString(k, x, y);
  lcd.setTextColor(vc, C_BG); lcd.drawString(v, x + 58, y);
}

static void drawEvents(int y, int h) {
  lcd.setTextDatum(lgfx::top_left); lcd.setTextSize(1);
  int rows = h / 11;
  for (int r = 0; r < rows; r++) {
    int yy = y + r * 11;
    lcd.fillRect(0, yy, W, 11, C_BG);
    if (r >= model.evtCount) continue;
    int idx = (model.evtHead - 1 - r + EVT_MAX * 2) % EVT_MAX;
    const Event& e = model.evt[idx];
    char b[72];
    uint32_t age = (millis() - e.ms) / 1000;
    snprintf(b, sizeof b, "%3lus %-4s %s", (unsigned long)age, e.src, e.text);
    lcd.setTextColor(e.warn ? C_WARN : C_CYAN, C_BG);
    lcd.drawString(b, 4, yy);
  }
}

// ---- pages ------------------------------------------------------------------
static void pageHome() {
  char b[40];
  // mini 433 bars
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("433 MHz", 4, BODY_Y + 2);
  int x0 = 4, y0 = BODY_Y + 12, bh = 40, bw = 6;
  lcd.fillRect(x0, y0, SUB_CH * (bw + 1), bh, C_PANEL);
  for (int i = 0; i < SUB_CH; i++) {
    int v = constrain(model.sub[i] + 120, 0, 90); int hh = v * bh / 90;
    lcd.fillRect(x0 + i * (bw + 1), y0 + bh - hh, bw, hh, hh > bh * 2 / 3 ? C_WARN : C_ACC);
  }
  // mini 2.4 band
  lcd.setTextColor(C_DIM, C_BG); lcd.drawString("2.4 GHz", 168, BODY_Y + 2);
  int x1 = 168, bw1 = 1;
  lcd.fillRect(x1, y0, NRF_CH * bw1 + 2, bh, C_PANEL);
  for (int i = 0; i < NRF_CH; i++) {
    int hh = model.nrf[i] * bh / 8;
    if (hh) lcd.fillRect(x1 + 1 + i, y0 + bh - hh, 1, hh, C_CYAN);
  }
  int y = y0 + bh + 6;
  snprintf(b, sizeof b, "%s %d/%d  C/N0 %.0f", model.fix ? "FIX" : "no fix", model.satsUsed, model.satsView, model.cn0Avg);
  label(4, y, "GPS", b, model.fix ? C_OK : C_DIM);
  snprintf(b, sizeof b, "%u APs (%u/%u)  BLE %u", model.wTotal, model.w24, model.w5, lble.seen);
  label(4, y + 12, "AIR", b);
  snprintf(b, sizeof b, "%s  up %lus  heap %lu", model.fw, (unsigned long)model.uptime, (unsigned long)model.freeHeap);
  label(4, y + 24, "NODE", b, model.linkAlive() ? C_TXT : C_WARN);
  lcd.drawFastHLine(0, y + 38, W, C_GRID);
  drawEvents(y + 42, BODY_Y + BODY_H - (y + 42));
}

static void pageSub() {
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("433.0 .. 435.0 MHz, 100 kHz steps, dBm", 4, BODY_Y + 2);
  int x0 = 24, y0 = BODY_Y + 14, gh = BODY_H - 34, gw = W - x0 - 4;
  int bw = gw / SUB_CH;
  lcd.fillRect(x0, y0, gw, gh, C_PANEL);
  for (int d = -110; d <= -40; d += 10) {
    int yy = y0 + gh - (d + 120) * gh / 90;
    lcd.drawFastHLine(x0, yy, gw, C_GRID);
    char b[6]; snprintf(b, sizeof b, "%d", d);
    lcd.setTextDatum(lgfx::middle_right); lcd.setTextColor(C_DIM, C_BG); lcd.drawString(b, x0 - 2, yy);
  }
  for (int i = 0; i < SUB_CH; i++) {
    int v = constrain(model.sub[i] + 120, 0, 90); int hh = v * gh / 90;
    int pk = constrain(model.subPeak[i] + 120, 0, 90); int ph = pk * gh / 90;
    int x = x0 + i * bw;
    lcd.fillRect(x + 1, y0 + gh - hh, bw - 2, hh, hh > gh * 2 / 3 ? C_WARN : C_ACC);
    lcd.drawFastHLine(x + 1, y0 + gh - ph, bw - 2, C_TXT);
    if (model.subPeak[i] > -120) model.subPeak[i]--;   // slow decay
  }
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("433.0", x0, y0 + gh + 2); lcd.drawString("433.9", x0 + 9 * bw, y0 + gh + 2); lcd.drawString("435.0", x0 + 19 * bw, y0 + gh + 2);
}

static void pageNrf() {
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("2400 .. 2525 MHz carrier hits per 8 sweeps (nRF24 RPD)", 4, BODY_Y + 2);
  int x0 = 4, y0 = BODY_Y + 14, gh = BODY_H - 46, gw = NRF_CH * 2 + 2;
  x0 = (W - gw) / 2;
  lcd.fillRect(x0, y0, gw, gh, C_PANEL);
  // Wi-Fi channel 1/6/11 centres: 2412/2437/2462
  for (int c : {12, 37, 62}) lcd.drawFastVLine(x0 + 1 + c * 2, y0, gh, C_GRID);
  for (int i = 0; i < NRF_CH; i++) {
    int hh = model.nrf[i] * gh / 8, ph = model.nrfPeak[i] * gh / 8;
    int x = x0 + 1 + i * 2;
    if (hh) lcd.fillRect(x, y0 + gh - hh, 2, hh, model.nrf[i] >= 6 ? C_WARN : C_CYAN);
    if (ph) lcd.drawFastHLine(x, y0 + gh - ph, 2, C_TXT);
    if (model.nrfPeak[i] && (millis() / 1000) % 4 == 0) model.nrfPeak[i]--;
  }
  lcd.setTextColor(C_DIM, C_BG); lcd.setTextDatum(lgfx::top_left);
  lcd.drawString("2400", x0, y0 + gh + 2); lcd.drawString("ch1", x0 + 24 - 8, y0 + gh + 2);
  lcd.drawString("ch6", x0 + 74 - 8, y0 + gh + 2); lcd.drawString("ch11", x0 + 124 - 10, y0 + gh + 2);
  lcd.drawString("2525", x0 + gw - 24, y0 + gh + 2);
  int busy = 0; for (int i = 0; i < NRF_CH; i++) if (model.nrf[i] >= 4) busy++;
  char b[48]; snprintf(b, sizeof b, "busy channels: %d   local BLE devices: %u", busy, lble.seen);
  lcd.drawString(b, 4, y0 + gh + 16);
}

static void pageGps() {
  char b[40]; int y = BODY_Y + 6;
  label(4, y, "fix", model.fix == 0 ? "none" : model.fix == 2 ? "DGPS" : "3D/2D", model.fix ? C_OK : C_WARN); y += 14;
  snprintf(b, sizeof b, "%d used / %d in view", model.satsUsed, model.satsView); label(4, y, "sats", b); y += 14;
  snprintf(b, sizeof b, "%.1f", model.hdop); label(4, y, "hdop", b); y += 14;
  snprintf(b, sizeof b, "%.6f", model.lat); label(4, y, "lat", b); y += 14;
  snprintf(b, sizeof b, "%.6f", model.lon); label(4, y, "lon", b); y += 14;
  snprintf(b, sizeof b, "%.0f m", model.alt); label(4, y, "alt", b); y += 14;
  snprintf(b, sizeof b, "avg %.0f  max %d dB-Hz", model.cn0Avg, model.cn0Max); label(4, y, "C/N0", b, model.cn0Avg < 25 && model.fix ? C_WARN : C_TXT); y += 14;
  label(4, y, "utc", model.utc); y += 18;
  lcd.setTextColor(C_DIM, C_BG); lcd.setTextDatum(lgfx::top_left);
  lcd.drawString("Interference watch: sudden drops in sats used or", 4, y);
  lcd.drawString("mean C/N0 raise a warn event (see HOME).", 4, y + 11);
  // C/N0 bar
  int bx = 200, by = BODY_Y + 8, bwid = 110, bh = 90;
  lcd.fillRect(bx, by, bwid, bh, C_PANEL);
  int hh = constrain((int)model.cn0Avg, 0, 50) * bh / 50;
  lcd.fillRect(bx + 10, by + bh - hh, 40, hh, model.cn0Avg < 25 ? C_WARN : C_OK);
  int hm = constrain((int)model.cn0Max, 0, 50) * bh / 50;
  lcd.fillRect(bx + 60, by + bh - hm, 40, hm, C_CYAN);
  lcd.setTextColor(C_DIM, C_BG); lcd.drawString("avg", bx + 20, by + bh + 2); lcd.drawString("max", bx + 70, by + bh + 2);
}

static void drawApList(int x, int y, const char* title, int n, std::function<void(int, char*, size_t)> row) {
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_ACC, C_BG); lcd.drawString(title, x, y);
  lcd.setTextColor(C_TXT, C_BG);
  for (int i = 0; i < 8; i++) {
    char b[48] = "";
    if (i < n) row(i, b, sizeof b);
    lcd.fillRect(x, y + 12 + i * 11, 156, 11, C_BG);
    lcd.drawString(b, x, y + 12 + i * 11);
  }
}

static void pageWifi() {
  char b[48];
  snprintf(b, sizeof b, "node: %u APs  2.4G %u  5G %u  best %d", model.wTotal, model.w24, model.w5, model.wStrongest);
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG); lcd.drawString(b, 4, BODY_Y + 2);
  snprintf(b, sizeof b, "this CYD: %u APs (2.4G only)  best %d%s", lwifi.total, lwifi.strongest, lwifi.scanning ? " ..." : "");
  lcd.drawString(b, 4, BODY_Y + 13);
  drawApList(4, BODY_Y + 28, "NODE (dual band)", model.wapN, [](int i, char* o, size_t n) {
    snprintf(o, n, "%3d ch%-3u %.14s", model.wap[i].rssi, model.wap[i].ch, model.wap[i].ssid);
  });
  drawApList(164, BODY_Y + 28, "CYD (2.4 GHz)", lwifi.topN, [](int i, char* o, size_t n) {
    snprintf(o, n, "%3d ch%-3u %.14s", lwifi.top[i].rssi, lwifi.top[i].ch, lwifi.top[i].ssid);
  });
}

static void pageBle() {
  char b[48];
  snprintf(b, sizeof b, "BLE (CYD radio, passive): %u devices / 5 s, best %d", lble.seen, lble.strongest);
  lcd.setTextDatum(lgfx::top_left); lcd.setTextColor(C_DIM, C_BG); lcd.drawString(b, 4, BODY_Y + 2);
  lcd.setTextColor(C_TXT, C_BG);
  for (int i = 0; i < 8; i++) {
    int y = BODY_Y + 16 + i * 12;
    lcd.fillRect(0, y, W, 12, C_BG);
    if (i >= lble.topN) continue;
    snprintf(b, sizeof b, "%3d  %s  %.18s", lble.top[i].rssi, lble.top[i].addr, lble.top[i].name[0] ? lble.top[i].name : "-");
    lcd.drawString(b, 4, y);
  }
  // rough proximity meter for the strongest device
  int bx = 4, by = BODY_Y + BODY_H - 30, bw = W - 8;
  lcd.fillRect(bx, by, bw, 10, C_PANEL);
  int v = constrain(lble.strongest + 100, 0, 70) * bw / 70;
  lcd.fillRect(bx, by, v, 10, lble.strongest > -50 ? C_WARN : C_CYAN);
  lcd.setTextColor(C_DIM, C_BG); lcd.drawString("strongest device proximity", bx, by + 12);
}

// ---- entry points ----------------------------------------------------------
void uiBegin() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(200);
  lcd.fillScreen(C_BG);
  lcd.setFont(&fonts::Font0);
  full = true;
}

void uiTick() {
  // touch: nav bar switches pages; tapping the body on HOME pings the node
  int32_t tx, ty;
  if (lcd.getTouch(&tx, &ty)) {
    static uint32_t lastTouch = 0;
    if (millis() - lastTouch > 300) {
      lastTouch = millis();
      if (ty >= H - NAV) { page = (Page)constrain(tx / (W / PG_COUNT), 0, PG_COUNT - 1); full = true; }
      else if (page == PG_HOME) protoSend("CMD,PING");
    }
  }
  if (!full && millis() - lastDraw < 250) return;
  lastDraw = millis();
  lcd.startWrite();
  if (full) { lcd.fillScreen(C_BG); drawNav(); clearBody(); full = false; }
  drawTop();
  switch (page) {
    case PG_HOME: pageHome(); break;
    case PG_SUB:  pageSub();  break;
    case PG_NRF:  pageNrf();  break;
    case PG_GPS:  pageGps();  break;
    case PG_WIFI: pageWifi(); break;
    case PG_BLE:  pageBle();  break;
    default: break;
  }
  lcd.endWrite();
}
