// Per-channel slow baseline (EWMA mean + variance) with deviation alerts.
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float mean, var;
    uint16_t n;        // samples seen (saturates)
    uint8_t hot;       // consecutive over-threshold samples
    bool alerting;
} baseline_ch_t;

typedef struct {
    baseline_ch_t *ch;
    int count;
    float alpha;       // EWMA weight for the mean (small = slow)
    float min_delta;   // dB above mean before a sample counts as hot
    float sigmas;      // or this many standard deviations, whichever is larger
    uint8_t hot_needed;// consecutive hot samples to raise an alert
} baseline_t;

void baseline_init(baseline_t *b, baseline_ch_t *storage, int count,
                   float alpha, float min_delta, float sigmas, uint8_t hot_needed);
// Feed one sample for a channel. Returns +1 when an alert starts, -1 when it
// clears, 0 otherwise. Alerts never fire before CONFIG_RFS_BASELINE_WARMUP samples.
int baseline_feed(baseline_t *b, int idx, float x);

#ifdef __cplusplus
}
#endif
