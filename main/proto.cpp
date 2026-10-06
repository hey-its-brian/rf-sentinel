#include "proto.h"
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#define HOST_UART UART_NUM_1
static SemaphoreHandle_t s_lock;
static char s_rxbuf[256];
static size_t s_rxlen;

void proto_init(void) {
    s_lock = xSemaphoreCreateMutex();
    uart_config_t cfg = {};
    cfg.baud_rate = CONFIG_RFS_HOST_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_driver_install(HOST_UART, 1024, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(HOST_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(HOST_UART, CONFIG_RFS_PIN_HOST_TX, CONFIG_RFS_PIN_HOST_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static uint8_t xor_sum(const char *p, size_t n) {
    uint8_t c = 0;
    for (size_t i = 0; i < n; i++) c ^= (uint8_t)p[i];
    return c;
}

void proto_send(const char *type, const char *fmt, ...) {
    char body[700];
    int n = snprintf(body, sizeof body, "RFS,%s", type);
    if (fmt && *fmt) {
        body[n++] = ',';
        va_list ap;
        va_start(ap, fmt);
        n += vsnprintf(body + n, sizeof body - n, fmt, ap);
        va_end(ap);
        if (n >= (int)sizeof body - 1) n = sizeof body - 2;
    }
    char frame[720];
    int m = snprintf(frame, sizeof frame, "$%s*%02X\n", body, xor_sum(body, n));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uart_write_bytes(HOST_UART, frame, m);
    fputs(frame, stdout);
    xSemaphoreGive(s_lock);
}

bool proto_poll_cmd(char *line, size_t len) {
    uint8_t ch;
    while (uart_read_bytes(HOST_UART, &ch, 1, 0) == 1) {
        if (ch == '\n' || ch == '\r') {
            if (s_rxlen == 0) continue;
            s_rxbuf[s_rxlen] = 0;
            strncpy(line, s_rxbuf, len - 1);
            line[len - 1] = 0;
            s_rxlen = 0;
            return true;
        }
        if (s_rxlen < sizeof s_rxbuf - 1) s_rxbuf[s_rxlen++] = (char)ch;
        else s_rxlen = 0;  // overflow, drop
    }
    return false;
}

const char *proto_check(char *line) {
    if (line[0] != '$') return NULL;
    char *star = strchr(line, '*');
    if (star) {
        unsigned want;
        if (sscanf(star + 1, "%2x", &want) != 1) return NULL;
        if (xor_sum(line + 1, star - line - 1) != (uint8_t)want) return NULL;
        *star = 0;
    }
    if (strncmp(line, "$RFS,", 5) != 0) return NULL;
    return line + 5;
}
