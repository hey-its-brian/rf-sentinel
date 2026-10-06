// RF Sentinel node firmware (M5Stamp C5). Receive-only multi-band monitor.
// Talks to a host (CYD, Tab5, laptop) over UART1 and mirrors every frame to
// the USB console. See docs/protocol.md.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "proto.h"
#include "baseline.h"
#include "radios.h"
#include "gps.h"
#include "wifiscan.h"

static const char *TAG = "rfs";

static radios_status_t s_radio;
static bool s_wifi_ok;
static bool s_quiet;                 // host asked us to stop streaming sweeps
static uint32_t s_sweep_ms = CONFIG_RFS_SWEEP_PERIOD_MS;

// Baselines
static baseline_ch_t s_sub_store[RFS_SUB_CHANNELS];
static baseline_t s_sub_base;
static baseline_ch_t s_nrf_store[RFS_NRF_CHANNELS];
static baseline_t s_nrf_base;
static baseline_ch_t s_gps_store[2];   // 0 = sats used, 1 = mean C/N0 (inverted: drops are alerts)
static baseline_t s_gps_base;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void send_hello(void) {
    proto_send("HELLO", "%s,%s,cc=%d,nrf=%d,lora=%d,gps=1,wifi=%d",
               RFS_FW_VERSION, RFS_PROTO_VERSION,
               s_radio.cc1101_ok, s_radio.nrf24_ok, s_radio.lora_ok, s_wifi_ok);
}

static void send_stat(void) {
    gps_fix_t g; gps_get(&g);
    bool gps_alive = g.sentences && (now_ms() - g.last_ms) < 3000;
    proto_send("STAT", "%lu,%d,%d,%d,%d,%lu", (unsigned long)(now_ms() / 1000),
               s_radio.cc1101_ok, s_radio.nrf24_ok, s_radio.lora_ok, gps_alive,
               (unsigned long)esp_get_free_heap_size());
}

static void send_gps(void) {
    gps_fix_t g; gps_get(&g);
    proto_send("GPS", "%d,%d,%d,%.1f,%.6f,%.6f,%.1f,%.1f,%d,%s",
               g.fix, g.sats_used, g.sats_view, g.hdop, g.lat, g.lon, g.alt_m,
               g.snr_avg, g.snr_max, g.utc[0] ? g.utc : "0");
    // GNSS interference watch: alert on sudden drops in satellites used or mean C/N0.
    // Feed negated values so a drop looks like a rise to the baseline logic.
    if (g.fix) {
        int r = baseline_feed(&s_gps_base, 0, -(float)g.sats_used);
        if (r > 0) proto_send("EVT", "warn,gps,sats dropped to %d", g.sats_used);
        if (r < 0) proto_send("EVT", "info,gps,sats recovered (%d)", g.sats_used);
        r = baseline_feed(&s_gps_base, 1, -g.snr_avg);
        if (r > 0) proto_send("EVT", "warn,gps,C/N0 dropped to %.0f dB", g.snr_avg);
        if (r < 0) proto_send("EVT", "info,gps,C/N0 recovered (%.0f dB)", g.snr_avg);
    }
}

static void do_sub_sweep(void) {
    int8_t v[RFS_SUB_CHANNELS];
    radios_sweep_sub(v);
    char buf[RFS_SUB_CHANNELS * 5 + 8]; int n = 0;
    for (int i = 0; i < RFS_SUB_CHANNELS; i++) {
        n += snprintf(buf + n, sizeof buf - n, "%s%d", i ? "," : "", v[i]);
        int r = baseline_feed(&s_sub_base, i, v[i]);
        if (r > 0) proto_send("EVT", "warn,sub,%.1f MHz active (%d dBm)", RFS_SUB_FIRST_MHZ + i * RFS_SUB_STEP_MHZ, v[i]);
        if (r < 0) proto_send("EVT", "info,sub,%.1f MHz quiet", RFS_SUB_FIRST_MHZ + i * RFS_SUB_STEP_MHZ);
    }
    if (!s_quiet) proto_send("SUB", "%d,%s", RFS_SUB_CHANNELS, buf);
}

// nRF sweeps are accumulated 8 at a time so each channel reports 0..8 hits
// as one hex digit: a 126-character string instead of 126 numbers.
static void do_nrf_sweep(void) {
    static uint8_t acc[RFS_NRF_CHANNELS];
    static int rounds;
    uint8_t v[RFS_NRF_CHANNELS];
    radios_sweep_nrf(v);
    for (int i = 0; i < RFS_NRF_CHANNELS; i++) acc[i] += v[i];
    if (++rounds < 8) return;
    char buf[RFS_NRF_CHANNELS + 1];
    for (int i = 0; i < RFS_NRF_CHANNELS; i++) {
        buf[i] = "0123456789ABCDEF"[acc[i] & 0xF];
        int r = baseline_feed(&s_nrf_base, i, acc[i]);
        if (r > 0) proto_send("EVT", "warn,nrf,%d MHz busy (%d/8)", 2400 + i, acc[i]);
        if (r < 0) proto_send("EVT", "info,nrf,%d MHz quiet", 2400 + i);
        acc[i] = 0;
    }
    buf[RFS_NRF_CHANNELS] = 0;
    rounds = 0;
    if (!s_quiet) proto_send("NRF", "%d,%s", RFS_NRF_CHANNELS, buf);
}

static void do_lora(void) {
    float rssi, snr;
    int n = radios_lora_poll(&rssi, &snr);
    if (n) proto_send("LORA", "%d,%.1f,%.1f", n, rssi, snr);
}

static void wifi_task(void *arg) {
    static uint16_t last_total;
    for (;;) {
        wifi_summary_t s;
        if (wifiscan_run(&s)) {
            proto_send("WIFI", "%u,%u,%u,%d", s.total, s.n24, s.n5, s.strongest);
            for (int i = 0; i < s.top_n; i++) {
                proto_send("WAP", "%02X%02X%02X%02X%02X%02X,%u,%d,%s",
                           s.top[i].bssid[0], s.top[i].bssid[1], s.top[i].bssid[2],
                           s.top[i].bssid[3], s.top[i].bssid[4], s.top[i].bssid[5],
                           s.top[i].channel, s.top[i].rssi, s.top[i].ssid[0] ? s.top[i].ssid : "<hidden>");
            }
            if (last_total && s.total > last_total + 3)
                proto_send("EVT", "info,wifi,%u new networks", s.total - last_total);
            last_total = s.total;
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_RFS_WIFI_PERIOD_MS));
    }
}

static void handle_cmd(char *line) {
    const char *p = proto_check(line);
    if (!p) return;
    if (!strncmp(p, "CMD,PING", 8))        proto_send("PONG", "%lu", (unsigned long)now_ms());
    else if (!strncmp(p, "CMD,HELLO", 9))  send_hello();
    else if (!strncmp(p, "CMD,QUIET", 9))  s_quiet = true;
    else if (!strncmp(p, "CMD,STREAM", 10)) s_quiet = false;
    else if (!strncmp(p, "CMD,RATE,", 9))  { int v = atoi(p + 9); if (v >= 100 && v <= 5000) s_sweep_ms = v; }
    else proto_send("ERR", "unknown command");
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "RF Sentinel %s starting", RFS_FW_VERSION);
    proto_init();
    gps_start();
    s_radio = radios_init();
    s_wifi_ok = CONFIG_RFS_WIFI_PERIOD_MS > 0 && wifiscan_init();

    baseline_init(&s_sub_base, s_sub_store, RFS_SUB_CHANNELS, 0.02f, 8.0f, 3.0f, 3);
    baseline_init(&s_nrf_base, s_nrf_store, RFS_NRF_CHANNELS, 0.02f, 3.0f, 3.0f, 2);
    baseline_init(&s_gps_base, s_gps_store, 2, 0.01f, 3.0f, 3.0f, 3);

    send_hello();
    if (s_wifi_ok) xTaskCreate(wifi_task, "wifi", 6144, NULL, 3, NULL);

    uint32_t last_sweep = 0, last_stat = 0;
    char cmd[256];
    for (;;) {
        uint32_t t = now_ms();
        if (t - last_sweep >= s_sweep_ms) {
            last_sweep = t;
            do_sub_sweep();
            do_nrf_sweep();
            do_lora();
        }
        if (t - last_stat >= CONFIG_RFS_STAT_PERIOD_MS) {
            last_stat = t;
            send_stat();
            send_gps();
        }
        while (proto_poll_cmd(cmd, sizeof cmd)) handle_cmd(cmd);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
