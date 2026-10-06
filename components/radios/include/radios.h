// Receive-only radio front end: CC1101 (433 MHz ISM), nRF24L01+ (2.4 GHz),
// optional SX1262 (LoRa 915). Everything here is a listener. There is no
// transmit call anywhere in this component; see RFS_RX_ONLY in radios.cpp.
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RFS_SUB_CHANNELS 21     // 433.00 .. 435.00 MHz in 0.1 MHz steps
#define RFS_NRF_CHANNELS 126    // 2400 .. 2525 MHz, 1 MHz steps
#define RFS_SUB_FIRST_MHZ 433.0f
#define RFS_SUB_STEP_MHZ 0.1f

typedef struct {
    bool cc1101_ok;
    bool nrf24_ok;
    bool lora_ok;
} radios_status_t;

// Bring up whatever answers on the bus. Missing modules are reported, not fatal.
radios_status_t radios_init(void);
radios_status_t radios_status(void);

// One RSSI sweep of the 433 band. out[i] in dBm (ints, typically -110..-30).
void radios_sweep_sub(int8_t out[RFS_SUB_CHANNELS]);

// One carrier-detect sweep of the 2.4 GHz band. out[ch] = 1 if the nRF24's
// RPD flag (energy above about -64 dBm) was set on that channel.
void radios_sweep_nrf(uint8_t out[RFS_NRF_CHANNELS]);

// LoRa listener: returns number of packets since last call and the latest
// packet RSSI/SNR (0 if none). Only meaningful when built with RFS_LORA.
int radios_lora_poll(float *rssi, float *snr);

#ifdef __cplusplus
}
#endif
