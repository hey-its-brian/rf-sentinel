// Wi-Fi presence scan using the C5's own dual-band radio (passive listen).
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_TOP_N 8

typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
} wifi_ap_t;

typedef struct {
    uint16_t total, n24, n5;
    int8_t strongest;
    wifi_ap_t top[WIFI_TOP_N];
    uint8_t top_n;
} wifi_summary_t;

bool wifiscan_init(void);
// Blocking scan of both bands. Returns false if the scan failed.
bool wifiscan_run(wifi_summary_t *out);

#ifdef __cplusplus
}
#endif
