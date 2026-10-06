// Minimal NMEA reader for ATGM336H / NEO-6M class modules (GGA, RMC, GSV).
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;         // RMC status A
    uint8_t fix;        // GGA fix quality 0/1/2
    uint8_t sats_used;  // GGA satellites in use
    uint8_t sats_view;  // GSV satellites in view (all constellations seen)
    float hdop;
    double lat, lon;    // decimal degrees
    float alt_m;
    float speed_kn;
    float snr_avg;      // mean C/N0 of satellites with a reading
    uint8_t snr_max;
    char utc[11];       // hhmmss.ss
    uint32_t sentences; // total sentences parsed (liveness)
    uint32_t last_ms;   // esp_timer ms of last valid sentence
} gps_fix_t;

void gps_start(void);
void gps_get(gps_fix_t *out);

#ifdef __cplusplus
}
#endif
