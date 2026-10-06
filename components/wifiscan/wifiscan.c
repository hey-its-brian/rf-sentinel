#include "wifiscan.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "soc/soc_caps.h"

static const char *TAG = "wifiscan";

bool wifiscan_init(void) {
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase(); r = nvs_flash_init();
    }
    if (r != ESP_OK) return false;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
#if SOC_WIFI_SUPPORT_5G
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
#endif
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi ready");
    return true;
}

bool wifiscan_run(wifi_summary_t *out) {
    memset(out, 0, sizeof *out);
    out->strongest = -127;
    wifi_scan_config_t sc = {
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.passive = 120,
    };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return false;
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0) { esp_wifi_clear_ap_list(); return true; }
    if (n > 64) n = 64;
    wifi_ap_record_t *recs = calloc(n, sizeof *recs);
    if (!recs) { esp_wifi_clear_ap_list(); return false; }
    esp_wifi_scan_get_ap_records(&n, recs);
    out->total = n;
    for (int i = 0; i < n; i++) {
        if (recs[i].primary > 14) out->n5++; else out->n24++;
        if (recs[i].rssi > out->strongest) out->strongest = recs[i].rssi;
    }
    // records come back sorted by RSSI; keep the top few
    out->top_n = n < WIFI_TOP_N ? n : WIFI_TOP_N;
    for (int i = 0; i < out->top_n; i++) {
        strncpy(out->top[i].ssid, (const char *)recs[i].ssid, 32);
        memcpy(out->top[i].bssid, recs[i].bssid, 6);
        out->top[i].channel = recs[i].primary;
        out->top[i].rssi = recs[i].rssi;
    }
    free(recs);
    return true;
}
