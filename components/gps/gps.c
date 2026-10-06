#include "gps.h"
#include <string.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define GPS_UART UART_NUM_0   // console is on USB-Serial-JTAG, so UART0 is free
static gps_fix_t s_fix;
static SemaphoreHandle_t s_lock;
// GSV accumulation across a multi-sentence group
static int s_snr_sum, s_snr_n, s_snr_max, s_view;

static double nmea_to_deg(const char *s, char hemi) {
    if (!*s) return 0;
    double v = atof(s);
    int deg = (int)(v / 100);
    double d = deg + (v - deg * 100) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -d : d;
}

static bool checksum_ok(const char *s) {
    if (*s != '$') return false;
    uint8_t c = 0; const char *p = s + 1;
    while (*p && *p != '*') c ^= (uint8_t)*p++;
    if (*p != '*') return true;  // no checksum, accept
    return (uint8_t)strtol(p + 1, NULL, 16) == c;
}

// Split in place on commas; fields[] points into line. Returns count.
static int split(char *line, char *fields[], int max) {
    int n = 0; char *p = line;
    char *star = strchr(p, '*'); if (star) *star = 0;
    while (n < max && p) { fields[n++] = p; p = strchr(p, ','); if (p) *p++ = 0; }
    return n;
}

static void handle(char *line) {
    if (!checksum_ok(line)) return;
    char *f[24]; int n = split(line, f, 24);
    if (n < 2 || strlen(f[0]) < 6) return;
    const char *type = f[0] + 3;  // skip $GP / $GN / $BD
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_fix.sentences++;
    s_fix.last_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (!strcmp(type, "GGA") && n >= 10) {
        strncpy(s_fix.utc, f[1], sizeof s_fix.utc - 1);
        s_fix.lat = nmea_to_deg(f[2], f[3][0]);
        s_fix.lon = nmea_to_deg(f[4], f[5][0]);
        s_fix.fix = atoi(f[6]);
        s_fix.sats_used = atoi(f[7]);
        s_fix.hdop = atof(f[8]);
        s_fix.alt_m = atof(f[9]);
    } else if (!strcmp(type, "RMC") && n >= 8) {
        s_fix.valid = (f[2][0] == 'A');
        s_fix.speed_kn = atof(f[7]);
    } else if (!strcmp(type, "GSV") && n >= 4) {
        int total = atoi(f[1]), idx = atoi(f[2]);
        if (idx == 1 && f[0][1] == 'G' && f[0][2] == 'P') { s_snr_sum = s_snr_n = s_snr_max = s_view = 0; }
        s_view += (idx == 1) ? atoi(f[3]) : 0;
        for (int i = 4; i + 3 < n; i += 4) {
            int snr = f[i + 3][0] ? atoi(f[i + 3]) : -1;
            if (snr >= 0) { s_snr_sum += snr; s_snr_n++; if (snr > s_snr_max) s_snr_max = snr; }
        }
        if (idx == total) {
            s_fix.sats_view = s_view;
            s_fix.snr_avg = s_snr_n ? (float)s_snr_sum / s_snr_n : 0;
            s_fix.snr_max = s_snr_max;
        }
    }
    xSemaphoreGive(s_lock);
}

static void gps_task(void *arg) {
    static char line[128]; size_t len = 0; uint8_t ch;
    for (;;) {
        if (uart_read_bytes(GPS_UART, &ch, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (ch == '\n' || ch == '\r') {
            if (len) { line[len] = 0; handle(line); len = 0; }
        } else if (len < sizeof line - 1) line[len++] = (char)ch;
        else len = 0;
    }
}

void gps_start(void) {
    s_lock = xSemaphoreCreateMutex();
    uart_config_t cfg = {
        .baud_rate = CONFIG_RFS_GPS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART,
        CONFIG_RFS_PIN_GPS_TX < 0 ? UART_PIN_NO_CHANGE : CONFIG_RFS_PIN_GPS_TX,
        CONFIG_RFS_PIN_GPS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(gps_task, "gps", 4096, NULL, 5, NULL);
}

void gps_get(gps_fix_t *out) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_fix;
    xSemaphoreGive(s_lock);
}
