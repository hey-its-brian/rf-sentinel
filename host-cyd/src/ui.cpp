// RF//SENTINEL CYD UI, drawn in the DECK//OS visual language of the Tab5
// cyberdeck (tab5-cyberdeck/components/deck_ui): near-black background,
// chamfered panels with thin outlines, a cyan primary neon with a magenta
// secondary, acid yellow for caution, red for alarms, Orbitron display type
// for titles and "// SECTION" headings, a monospace face for data.
//
// Everything is drawn straight to the panel. Static chrome is drawn once per
// page change; the 250 ms refresh only clears and repaints the regions whose
// data can change, so nothing flashes.
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
static bool full = true;        // page changed: redraw its static chrome
static bool chromeDrawn = false; // header title and rule are drawn once
static uint32_t pingMs = 0;     // last tap-to-ping, for on-screen feedback

// ---- DECK//OS palette (deck_theme.c, NETRUNNER accent) ----------------------
static constexpr uint16_t rgb(uint32_t c) {
  return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}
static constexpr uint32_t mix(uint32_t a, uint32_t b, unsigned t) {  // t/255 of a over b
  return ((((a >> 16) & 0xFF) * t + ((b >> 16) & 0xFF) * (255 - t)) / 255) << 16 |
         ((((a >> 8) & 0xFF) * t + ((b >> 8) & 0xFF) * (255 - t)) / 255) << 8 |
         (((a & 0xFF) * t + (b & 0xFF) * (255 - t)) / 255);
}
static constexpr uint16_t C_BG      = rgb(0x07070D);  // screen background
static constexpr uint16_t C_GRID    = rgb(0x16182A);  // grid lines
static constexpr uint16_t C_PANEL   = rgb(0x0D0F1C);  // panel fill
static constexpr uint16_t C_PANELHI = rgb(0x171A30);  // raised / selected fill
static constexpr uint16_t C_LINE    = rgb(0x2A3050);  // idle outline, rules
static constexpr uint16_t C_TXT     = rgb(0xD8E1F0);  // primary text
static constexpr uint16_t C_DIM     = rgb(0x7A84A3);  // secondary text (web value, legible at 6x8)
static constexpr uint16_t C_ACC     = rgb(0x00F0FF);  // primary neon
static constexpr uint16_t C_ACC2    = rgb(0xFF2A6D);  // secondary neon
static constexpr uint16_t C_WARN    = rgb(0xF3E600);  // caution
static constexpr uint16_t C_OK      = rgb(0x39FF14);  // healthy
static constexpr uint16_t C_DANGER  = rgb(0xFF3B3B);  // alarm
static constexpr uint16_t C_RULE    = rgb(mix(0x00F0FF, 0x0D0F1C, 102));  // accent at 40%

// ---- layout (landscape 320x240) ---------------------------------------------
static const int W = 320, H = 240;
static const int TOP = 22;                       // status bar incl. its rule
static const int NAV_H = 26, NAV_Y = H - NAV_H;  // bottom nav
static const int BODY_Y = TOP + 2, BODY_B = NAV_Y - 2;  // body rows [BODY_Y, BODY_B)
static const int TAB_W = 53;                     // 6 nav tabs across 318 px

static const char* NAMES[PG_COUNT] = {"HOME", "433", "2.4G", "GPS", "WIFI", "BLE"};
static const char* TITLES[PG_COUNT] = {"HOME", "433 MHZ", "2.4 GHZ", "GNSS", "WI-FI", "BLE"};

// Chamfer flags, same meaning as DECK_CUT_* in deck_widgets.h
enum : uint8_t { CUT_TL = 1, CUT_TR = 2, CUT_BR = 4, CUT_BL = 8, CUT_DIAG = CUT_TL | CUT_BR };

// ---- primitives -------------------------------------------------------------
// Chamfered panel: fill, corners knocked out to the backdrop, 1 px outline,
// optional secondary-neon tab on the top edge (the DECK module tile look).
static void panel(int x, int y, int w, int h, uint8_t cuts, int c, uint16_t fill, uint16_t line,
                  uint16_t backdrop = C_BG, bool tab = false, uint16_t tabCol = C_ACC2) {
  int x2 = x + w - 1, y2 = y + h - 1;
  lcd.fillRect(x, y, w, h, fill);
  if (cuts & CUT_TL) lcd.fillTriangle(x, y, x + c, y, x, y + c, backdrop);
  if (cuts & CUT_TR) lcd.fillTriangle(x2, y, x2 - c, y, x2, y + c, backdrop);
  if (cuts & CUT_BR) lcd.fillTriangle(x2, y2, x2 - c, y2, x2, y2 - c, backdrop);
  if (cuts & CUT_BL) lcd.fillTriangle(x, y2, x + c, y2, x, y2 - c, backdrop);
  int tl = (cuts & CUT_TL) ? c : 0, tr = (cuts & CUT_TR) ? c : 0;
  int br = (cuts & CUT_BR) ? c : 0, bl = (cuts & CUT_BL) ? c : 0;
  lcd.drawFastHLine(x + tl, y, w - tl - tr, line);
  lcd.drawFastHLine(x + bl, y2, w - bl - br, line);
  lcd.drawFastVLine(x, y + tl, h - tl - bl, line);
  lcd.drawFastVLine(x2, y + tr, h - tr - br, line);
  if (tl) lcd.drawLine(x, y + c, x + c, y, line);
  if (tr) lcd.drawLine(x2 - c, y, x2, y + c, line);
  if (br) lcd.drawLine(x2, y2 - c, x2 - c, y2, line);
  if (bl) lcd.drawLine(x + c, y2, x, y2 - c, line);
  if (tab) lcd.fillRect(x + tl + 4, y, 14, 2, tabCol);
}

// Orbitron text: LovyanGFX ships Orbitron Light only at 24 px, which is far
// too big for this panel, so render it into a short-lived sprite and push it
// down-scaled with anti-aliasing. Uppercase only (the sprite is cut to cap
// height). Returns the drawn width in screen pixels.
struct Seg { const char* s; uint16_t c; };
static int orb(int x, int y, float z, uint16_t bg, std::initializer_list<Seg> segs) {
  LGFX_Sprite spr(&lcd);
  spr.setColorDepth(16);
  spr.setFont(&fonts::Orbitron_Light_24);
  int wsum = 0;
  for (const Seg& g : segs) wsum += spr.textWidth(g.s);
  const int sh = 20, base = 18;  // caps sit on rows 1..17
  if (!spr.createSprite(wsum + 2, sh)) {
    // Low heap: fall back to the data font so the label still shows.
    lcd.setFont(&fonts::Font0); lcd.setTextDatum(lgfx::top_left);
    int cx = x;
    for (const Seg& g : segs) { lcd.setTextColor(g.c, bg); cx += lcd.drawString(g.s, cx, y + 1); }
    return cx - x;
  }
  spr.fillSprite(bg);
  spr.setTextDatum(lgfx::baseline_left);
  int cx = 1;
  for (const Seg& g : segs) { spr.setTextColor(g.c); cx += spr.drawString(g.s, cx, base); }
  spr.setPivot(0, 0);
  spr.pushRotateZoomWithAA(&lcd, x, y, 0, z, z);
  spr.deleteSprite();
  lcd.setFont(&fonts::Font0);
  return (int)((wsum + 2) * z + 0.5f);
}
static const float Z_HEAD = 0.5f;   // headings and nav labels: ~9 px caps
static const int HEAD_H = 10;

// Data text in the 6x8 mono face, background-filled to clearW (0 = no clear).
static int txt(int x, int y, const char* s, uint16_t fg, uint16_t bg = C_BG, int clearW = 0,
               lgfx::textdatum_t d = lgfx::top_left) {
  lcd.setFont(&fonts::Font0);
  if (clearW) {
    int cx = (d == lgfx::top_right) ? x - clearW : (d == lgfx::top_center) ? x - clearW / 2 : x;
    lcd.fillRect(cx, y, clearW, 8, bg);
  }
  lcd.setTextDatum(d);
  lcd.setTextColor(fg, bg);
  return lcd.drawString(s, x, y);
}

// Page title in the DECK status bar style: "02" accent, "//" dim, name in
// text colour, then a rule out to an optional dim detail on the right.
static void pageTitle(const char* right) {
  char num[4]; snprintf(num, sizeof num, "%02d", (int)page + 1);
  int y = BODY_Y + 1;
  int w = orb(4, y, Z_HEAD, C_BG, {{num, C_ACC}, {" // ", C_DIM}, {TITLES[page], C_TXT}});
  int rx = W - 4;
  if (right) rx -= txt(W - 4, y + 1, right, C_DIM, C_BG, 0, lgfx::top_right) + 5;
  if (rx > 4 + w + 5) lcd.drawFastHLine(4 + w + 5, y + 5, rx - (4 + w + 5), C_LINE);
}

// "// SECTION" heading: accent Orbitron, then a line-colour rule.
static void section(int x, int y, int w, const char* title, const char* right = nullptr,
                    uint16_t bg = C_BG) {
  char b[40]; snprintf(b, sizeof b, "// %s", title);
  int tw = orb(x, y, Z_HEAD, bg, {{b, C_ACC}});
  int rx = x + w;
  if (right) rx -= txt(x + w, y + 1, right, C_DIM, bg, 0, lgfx::top_right) + 5;
  if (rx > x + tw + 5) lcd.drawFastHLine(x + tw + 5, y + 5, rx - (x + tw + 5), C_LINE);
}

// Key in dim, value in colour; the value field is cleared to valW first.
static void kv(int x, int y, const char* k, const char* v, uint16_t vc = C_TXT, int kw = 36,
               int valW = 0, uint16_t bg = C_BG) {
  txt(x, y, k, C_DIM, bg);
  txt(x + kw, y, v, vc, bg, valW ? valW : (int)strlen(v) * 6);
}

// One bar-graph column: empty part in panel colour with grid ticks, then
// the bar. Drawn per column so the graph never blanks as a whole.
static void column(int x, int w, int y0, int gh, int hh, uint16_t col, const int* grid = nullptr,
                   int ng = 0) {
  hh = constrain(hh, 0, gh);
  int top = y0 + gh - hh;
  if (hh < gh) {
    lcd.fillRect(x, y0, w, gh - hh, C_PANEL);
    for (int i = 0; i < ng; i++) if (grid[i] < top) lcd.drawFastHLine(x, grid[i], w, C_GRID);
  }
  if (hh) lcd.fillRect(x, top, w, hh, col);
}

// ---- status bar -------------------------------------------------------------
// Outlined chip with a cut bottom-right corner (deck_chip), drawn leftwards
// from xr. Returns its left edge.
static int chip(int xr, const char* s, uint16_t c) {
  int w = (int)strlen(s) * 6 + 4, x = xr - w;
  panel(x, 4, w, 13, CUT_BR, 3, C_PANEL, c, C_PANEL);
  txt(x + 2, 7, s, c, C_PANEL);
  return x;
}

static void drawTop() {
  if (!chromeDrawn) {
    lcd.fillRect(0, 0, W, TOP, C_PANEL);
    orb(6, 5, 0.55f, C_PANEL, {{"RF", C_TXT}, {"//", C_ACC}, {"SENTINEL", C_TXT}});
    // DECK status bar rule: dim accent line, bright accent and accent2 segments.
    lcd.drawFastHLine(0, TOP - 1, W, C_RULE);
    lcd.fillRect(0, TOP - 2, 55, 2, C_ACC);
    lcd.fillRect(58, TOP - 2, 15, 2, C_ACC2);
    chromeDrawn = true;
  }
  // Chips only repaint when their content or colour changes.
  bool link = model.linkAlive();
  char sats[12], warns[12];
  snprintf(sats, sizeof sats, "%dSAT", model.satsUsed);
  snprintf(warns, sizeof warns, "!%lu", (unsigned long)model.warnCount);
  char sig[64];
  snprintf(sig, sizeof sig, "%d%d%d%d%d%s%s", link, model.ccOk, model.nrfOk, model.gpsOk, model.fix != 0, sats, warns);
  static char last[64] = "";
  if (!strcmp(sig, last)) return;
  strlcpy(last, sig, sizeof last);
  lcd.fillRect(125, 2, W - 125, TOP - 4, C_PANEL);
  int x = W - 4;
  x = chip(x, warns, model.warnCount ? C_DANGER : C_DIM) - 3;
  x = chip(x, sats, model.fix ? C_OK : C_DIM) - 3;
  x = chip(x, "GPS", model.gpsOk ? C_ACC : C_DIM) - 3;
  x = chip(x, "NRF", model.nrfOk ? C_ACC : C_DIM) - 3;
  x = chip(x, "CC", model.ccOk ? C_ACC : C_DIM) - 3;
  chip(x, link ? "NODE" : "NO NODE", link ? C_OK : C_DANGER);
}

// ---- bottom nav ---------------------------------------------------------------
static void drawNav() {
  lcd.fillRect(0, NAV_Y, W, NAV_H, C_BG);
  for (int i = 0; i < PG_COUNT; i++) {
    bool on = i == page;
    int x = 1 + i * TAB_W, w = TAB_W - 2, y = NAV_Y + 1, h = NAV_H - 2;
    panel(x, y, w, h, CUT_DIAG, 5, on ? C_PANELHI : C_PANEL, on ? C_ACC : C_LINE, C_BG, on, C_ACC2);
    LGFX_Sprite m(&lcd); m.setFont(&fonts::Orbitron_Light_24);
    int tw = (int)((m.textWidth(NAMES[i]) + 2) * Z_HEAD);
    orb(x + (w - tw) / 2, y + (h - HEAD_H) / 2 + 1, Z_HEAD, on ? C_PANELHI : C_PANEL,
        {{NAMES[i], on ? C_ACC : C_TXT}});
  }
}

// ---- pages ------------------------------------------------------------------
static void drawEvents(int y, int rows, int pitch) {
  char b[72];
  for (int r = 0; r < rows; r++) {
    int yy = y + r * pitch;
    lcd.fillRect(0, yy, W, 8, C_BG);
    if (r >= model.evtCount) continue;
    int idx = (model.evtHead - 1 - r + EVT_MAX * 2) % EVT_MAX;
    const Event& e = model.evt[idx];
    uint32_t age = (millis() - e.ms) / 1000;
    snprintf(b, sizeof b, "%3lus", (unsigned long)age);
    int x = 4 + txt(4, yy, b, C_DIM);
    x += txt(x, yy, e.warn ? " !" : "  ", C_DANGER);
    snprintf(b, sizeof b, "%-4s ", e.src);
    x += txt(x, yy, b, e.warn ? C_DANGER : C_ACC);
    txt(x, yy, e.text, e.warn ? C_DANGER : C_TXT);
  }
}

static void pageHome(bool f) {
  const int py = BODY_Y + 15, ph = 50;           // mini spectrum panels
  const int by = py + 13, bh = ph - 17;          // bar area inside them
  const int ky = py + ph + 4;                    // key/value block
  const int ey = ky + 32;                        // event log heading
  if (f) {
    pageTitle("NODE + LOCAL RADIOS");
    panel(4, py, 154, ph, CUT_DIAG, 6, C_PANEL, C_LINE, C_BG, true);
    panel(162, py, 154, ph, CUT_DIAG, 6, C_PANEL, C_LINE, C_BG, true);
    txt(10, py + 4, "433 MHZ", C_DIM, C_PANEL);
    txt(168, py + 4, "2.4 GHZ", C_DIM, C_PANEL);
    section(4, ey, W - 8, "EVENT LOG", "TAP: PING");
  }
  // 433 mini bars
  for (int i = 0; i < SUB_CH; i++) {
    int hh = constrain(model.sub[i] + 120, 0, 90) * bh / 90;
    column(8 + i * 7, 6, by, bh, hh, hh > bh * 2 / 3 ? C_WARN : C_ACC);
  }
  // 2.4 GHz mini carrier map
  for (int i = 0; i < NRF_CH; i++) {
    int hh = model.nrf[i] * bh / 8;
    column(176 + i, 1, by, bh, hh, model.nrf[i] >= 6 ? C_WARN : C_ACC);
  }
  char b[48];
  snprintf(b, sizeof b, "%s %d/%d  C/N0 %.0f", model.fix ? "FIX" : "NO FIX", model.satsUsed, model.satsView, model.cn0Avg);
  kv(4, ky, "GPS", b, model.fix ? C_OK : C_WARN, 36, 276);
  snprintf(b, sizeof b, "%u APS (%u/%u)  BLE %u", model.wTotal, model.w24, model.w5, lble.seen);
  kv(4, ky + 10, "AIR", b, C_TXT, 36, 276);
  snprintf(b, sizeof b, "%s  UP %lus  HEAP %lu", model.fw, (unsigned long)model.uptime, (unsigned long)model.freeHeap);
  kv(4, ky + 20, "NODE", b, model.linkAlive() ? C_TXT : C_DANGER, 36, 276);
  // touch hint, flips to an acknowledgement after a tap
  bool sent = pingMs && millis() - pingMs < 1500;
  txt(W - 4, ey + 1, sent ? "PING SENT" : "TAP: PING", sent ? C_ACC : C_DIM, C_BG, 54, lgfx::top_right);
  drawEvents(ey + HEAD_H + 4, EVT_MAX, 9);
}

static void pageSub(bool f) {
  const int px = 27, py = BODY_Y + 15, pw = W - 4 - px, ph = 156;
  const int bw = (pw - 6) / SUB_CH;                  // 13 px per 100 kHz step
  const int x0 = px + (pw - bw * SUB_CH) / 2, y0 = py + 3, gh = ph - 6;
  int grid[8], ng = 0;
  for (int d = -110; d <= -40; d += 10) grid[ng++] = y0 + gh - (d + 120) * gh / 90;
  if (f) {
    pageTitle("100 KHZ STEPS / DBM");
    panel(px, py, pw, ph, CUT_DIAG, 8, C_PANEL, C_LINE);
    for (int i = 0, d = -110; i < ng; i++, d += 10) {
      char b[6]; snprintf(b, sizeof b, "%d", d);
      lcd.setTextDatum(lgfx::middle_right); lcd.setTextColor(i & 1 ? C_DIM : C_LINE, C_BG);
      lcd.drawString(b, px - 2, grid[i]);
    }
    int ly = py + ph + 3;
    txt(x0, ly, "433.0", C_DIM);
    txt(x0 + 9 * bw + bw / 2, ly, "433.9", C_DIM, C_BG, 0, lgfx::top_center);
    txt(x0 + SUB_CH * bw, ly, "435.0 MHZ", C_DIM, C_BG, 0, lgfx::top_right);
  }
  for (int i = 0; i < SUB_CH; i++) {
    int hh = constrain(model.sub[i] + 120, 0, 90) * gh / 90;
    int pk = constrain(model.subPeak[i] + 120, 0, 90) * gh / 90;
    int x = x0 + i * bw;
    column(x + 1, bw - 2, y0, gh, hh, hh > gh * 2 / 3 ? C_WARN : C_ACC, grid, ng);
    if (pk > 0) lcd.drawFastHLine(x + 1, y0 + gh - pk, bw - 2, C_ACC2);   // peak hold
    if (model.subPeak[i] > -120) model.subPeak[i]--;   // slow decay
  }
}

static void pageNrf(bool f) {
  const int gw = NRF_CH * 2, pw = gw + 6, px = (W - pw) / 2, py = BODY_Y + 27, ph = 118;
  const int x0 = px + 3, y0 = py + 3, gh = ph - 6;
  static const int CH[3] = {12, 37, 62};   // Wi-Fi ch 1/6/11 centres: 2412/2437/2462
  int grid[3] = {y0 + gh / 4, y0 + gh / 2, y0 + gh * 3 / 4};
  if (f) {
    pageTitle("NRF24 RPD");
    txt(4, BODY_Y + 15, "2400 .. 2525 MHZ  CARRIER HITS PER 8 SWEEPS", C_DIM);
    panel(px, py, pw, ph, CUT_DIAG, 8, C_PANEL, C_LINE);
    int ly = py + ph + 3;
    txt(x0, ly, "2400", C_DIM);
    txt(x0 + CH[0] * 2, ly, "CH1", C_ACC2, C_BG, 0, lgfx::top_center);
    txt(x0 + CH[1] * 2, ly, "CH6", C_ACC2, C_BG, 0, lgfx::top_center);
    txt(x0 + CH[2] * 2, ly, "CH11", C_ACC2, C_BG, 0, lgfx::top_center);
    txt(x0 + gw, ly, "2525", C_DIM, C_BG, 0, lgfx::top_right);
  }
  for (int i = 0; i < NRF_CH; i++) {
    int hh = model.nrf[i] * gh / 8, pk = model.nrfPeak[i] * gh / 8;
    int x = x0 + i * 2;
    column(x, 2, y0, gh, hh, model.nrf[i] >= 6 ? C_WARN : C_ACC, grid, 3);
    for (int c : CH)
      if (c == i) for (int yy = y0; yy < y0 + gh - hh; yy += 3) lcd.drawPixel(x, yy, C_ACC2);
    if (pk) lcd.drawFastHLine(x, y0 + gh - pk, 2, C_TXT);
    if (model.nrfPeak[i] && (millis() / 1000) % 4 == 0) model.nrfPeak[i]--;
  }
  int busy = 0; for (int i = 0; i < NRF_CH; i++) if (model.nrf[i] >= 4) busy++;
  char b[12];
  int y = py + ph + 15;
  snprintf(b, sizeof b, "%d", busy);
  kv(4, y, "BUSY CHANNELS", b, busy ? C_WARN : C_TXT, 84, 30);
  snprintf(b, sizeof b, "%u", lble.seen);
  kv(170, y, "LOCAL BLE DEVICES", b, C_TXT, 108, 30);
}

static void pageGps(bool f) {
  const int px = 196, py = BODY_Y + 15, pw = W - 4 - px, ph = 112;
  const int y0 = py + 16, gh = ph - 30;      // C/N0 bars, 0..50 dB-Hz
  const int bx1 = px + 16, bx2 = px + 66, bwid = 36;
  int grid[4]; for (int i = 0; i < 4; i++) grid[i] = y0 + gh - (i + 1) * 10 * gh / 50;
  const int sy = py + ph + 8;
  if (f) {
    pageTitle(nullptr);
    panel(px, py, pw, ph, CUT_DIAG, 8, C_PANEL, C_LINE, C_BG, true);
    txt(px + 8, py + 5, "C/N0 DB-HZ", C_DIM, C_PANEL);
    txt(bx1 + bwid / 2, y0 + gh + 3, "AVG", C_DIM, C_PANEL, 0, lgfx::top_center);
    txt(bx2 + bwid / 2, y0 + gh + 3, "MAX", C_DIM, C_PANEL, 0, lgfx::top_center);
    section(4, sy, W - 8, "INTERFERENCE WATCH");
    txt(4, sy + HEAD_H + 5, "Sudden drops in sats used or mean C/N0", C_DIM);
    txt(4, sy + HEAD_H + 15, "raise a warn event (see HOME).", C_DIM);
  }
  char b[40]; int y = py + 2; const int vw = 150;
  bool weak = model.cn0Avg < 25 && model.fix;
  kv(4, y, "FIX", model.fix == 0 ? "NONE" : model.fix == 2 ? "DGPS" : "3D/2D", model.fix ? C_OK : C_WARN, 40, vw); y += 13;
  snprintf(b, sizeof b, "%d USED / %d IN VIEW", model.satsUsed, model.satsView); kv(4, y, "SATS", b, C_TXT, 40, vw); y += 13;
  snprintf(b, sizeof b, "%.1f", model.hdop); kv(4, y, "HDOP", b, C_TXT, 40, vw); y += 13;
  snprintf(b, sizeof b, "%.6f", model.lat); kv(4, y, "LAT", b, C_TXT, 40, vw); y += 13;
  snprintf(b, sizeof b, "%.6f", model.lon); kv(4, y, "LON", b, C_TXT, 40, vw); y += 13;
  snprintf(b, sizeof b, "%.0f M", model.alt); kv(4, y, "ALT", b, C_TXT, 40, vw); y += 13;
  snprintf(b, sizeof b, "AVG %.0f  MAX %d DB-HZ", model.cn0Avg, model.cn0Max); kv(4, y, "C/N0", b, weak ? C_WARN : C_TXT, 40, vw); y += 13;
  kv(4, y, "UTC", model.utc, C_TXT, 40, vw);
  // C/N0 bars with a dashed caution line at 25 dB-Hz
  int ha = constrain((int)model.cn0Avg, 0, 50) * gh / 50;
  int hm = constrain((int)model.cn0Max, 0, 50) * gh / 50;
  for (int i = 0; i < bwid; i += 4) {
    column(bx1 + i, 4, y0, gh, ha, model.cn0Avg < 25 ? C_WARN : C_OK, grid, 4);
    column(bx2 + i, 4, y0, gh, hm, C_ACC, grid, 4);
  }
  int ty = y0 + gh - 25 * gh / 50;
  for (int x = bx1; x < bx2 + bwid; x += 4) {
    bool inA = x < bx1 + bwid, inM = x >= bx2;
    bool covered = (inA && ty >= y0 + gh - ha) || (inM && ty >= y0 + gh - hm);
    if (!covered && (inA || inM)) lcd.drawFastHLine(x, ty, 2, C_WARN);
  }
}

static void apPanel(int x, int y, int w, int h, const char* title, int n,
                    std::function<void(int, char*, size_t)> row, bool f) {
  if (f) {
    panel(x, y, w, h, CUT_DIAG, 6, C_PANEL, C_LINE, C_BG, true);
    section(x + 6, y + 6, w - 12, title, nullptr, C_PANEL);
  }
  for (int i = 0; i < 8; i++) {
    char b[48] = "";
    if (i < n) row(i, b, sizeof b);
    int yy = y + 22 + i * 15;
    lcd.fillRect(x + 4, yy, w - 8, 8, C_PANEL);
    if (!b[0]) continue;
    b[3] = 0;   // rows start with a 3-wide RSSI, drawn in the accent
    txt(x + 5, yy, b, C_ACC, C_PANEL);
    txt(x + 5 + 24, yy, b + 4, C_TXT, C_PANEL);
  }
}

static void pageWifi(bool f) {
  if (f) pageTitle("NODE VS CYD");
  char b[48];
  snprintf(b, sizeof b, "%u APS  2.4G %u  5G %u  BEST %d", model.wTotal, model.w24, model.w5, model.wStrongest);
  kv(4, BODY_Y + 15, "NODE", b, C_TXT, 30, 282);
  snprintf(b, sizeof b, "%u APS (2.4G ONLY)  BEST %d%s", lwifi.total, lwifi.strongest, lwifi.scanning ? "  SCAN.." : "");
  kv(4, BODY_Y + 25, "CYD", b, lwifi.scanning ? C_ACC : C_TXT, 30, 282);
  const int py = BODY_Y + 38, ph = BODY_B - py;
  apPanel(4, py, 154, ph, "NODE 2.4+5G", model.wapN, [](int i, char* o, size_t n) {
    snprintf(o, n, "%3d ch%-3u %.14s", model.wap[i].rssi, model.wap[i].ch, model.wap[i].ssid);
  }, f);
  apPanel(162, py, 154, ph, "CYD 2.4G", lwifi.topN, [](int i, char* o, size_t n) {
    snprintf(o, n, "%3d ch%-3u %.14s", lwifi.top[i].rssi, lwifi.top[i].ch, lwifi.top[i].ssid);
  }, f);
}

static void pageBle(bool f) {
  const int py = BODY_Y + 27, ph = 120;
  const int sy = py + ph + 5, segY = sy + HEAD_H + 5;
  const int NSEG = 31;
  if (f) {
    pageTitle("PASSIVE / CYD RADIO");
    panel(4, py, W - 8, ph, CUT_DIAG, 8, C_PANEL, C_LINE, C_BG, true);
    section(4, sy, W - 8, "PROXIMITY", "STRONGEST DEVICE");
  }
  char b[48];
  snprintf(b, sizeof b, "%u DEVICES / 5 S", lble.seen);
  kv(4, BODY_Y + 15, "SEEN", b, C_TXT, 30, 120);
  snprintf(b, sizeof b, "%d DBM", lble.strongest);
  kv(180, BODY_Y + 15, "BEST", b, lble.strongest > -50 ? C_WARN : C_TXT, 30, 70);
  for (int i = 0; i < 8; i++) {
    int y = py + 8 + i * 14;
    lcd.fillRect(8, y, W - 16, 8, C_PANEL);
    if (i >= lble.topN) continue;
    snprintf(b, sizeof b, "%3d", lble.top[i].rssi);
    txt(10, y, b, lble.top[i].rssi > -50 ? C_WARN : C_ACC, C_PANEL);
    snprintf(b, sizeof b, "%s  %.18s", lble.top[i].addr, lble.top[i].name[0] ? lble.top[i].name : "-");
    txt(10 + 30, y, b, C_TXT, C_PANEL);
  }
  // segmented proximity meter for the strongest device
  int on = constrain(lble.strongest + 100, 0, 70) * NSEG / 70;
  uint16_t c = lble.strongest > -50 ? C_WARN : C_ACC;
  for (int i = 0; i < NSEG; i++) lcd.fillRect(5 + i * 10, segY, 8, 12, i < on ? c : C_LINE);
}

// ---- entry points ----------------------------------------------------------
void uiBegin() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(200);
  lcd.fillScreen(C_BG);
  lcd.setFont(&fonts::Font0);
  full = true;
  chromeDrawn = false;
}

void uiTick() {
  // touch: nav bar switches pages; tapping the body on HOME pings the node
  int32_t tx, ty;
  if (lcd.getTouch(&tx, &ty)) {
    static uint32_t lastTouch = 0;
    if (millis() - lastTouch > 300) {
      lastTouch = millis();
      if (ty >= NAV_Y) {
        Page p = (Page)constrain(tx / TAB_W, 0, PG_COUNT - 1);
        if (p != page) { page = p; full = true; }
      } else if (page == PG_HOME) {
        protoSend("CMD,PING");
        pingMs = millis();
      }
    }
  }
  if (!full && millis() - lastDraw < 250) return;
  lastDraw = millis();
  bool f = full;
  full = false;
  lcd.startWrite();
  drawTop();
  if (f) {
    drawNav();
    lcd.fillRect(0, BODY_Y, W, BODY_B - BODY_Y, C_BG);
  }
  switch (page) {
    case PG_HOME: pageHome(f); break;
    case PG_SUB:  pageSub(f);  break;
    case PG_NRF:  pageNrf(f);  break;
    case PG_GPS:  pageGps(f);  break;
    case PG_WIFI: pageWifi(f); break;
    case PG_BLE:  pageBle(f);  break;
    default: break;
  }
  lcd.endWrite();
}
