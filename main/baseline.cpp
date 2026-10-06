#include "baseline.h"
#include <math.h>
#include "sdkconfig.h"

void baseline_init(baseline_t *b, baseline_ch_t *storage, int count,
                   float alpha, float min_delta, float sigmas, uint8_t hot_needed) {
    b->ch = storage; b->count = count; b->alpha = alpha;
    b->min_delta = min_delta; b->sigmas = sigmas; b->hot_needed = hot_needed;
    for (int i = 0; i < count; i++) storage[i] = (baseline_ch_t){0, 0, 0, 0, false};
}

int baseline_feed(baseline_t *b, int idx, float x) {
    if (idx < 0 || idx >= b->count) return 0;
    baseline_ch_t *c = &b->ch[idx];
    if (c->n == 0) { c->mean = x; c->var = 0; }
    float d = x - c->mean;
    float thresh = fmaxf(b->min_delta, b->sigmas * sqrtf(c->var));
    bool hot = d > thresh;
    // Only let quiet samples shape the baseline so a long burst does not become "normal" quickly.
    float a = hot ? b->alpha * 0.1f : b->alpha;
    c->mean += a * d;
    c->var = (1 - a) * (c->var + a * d * d);
    if (c->n < 65535) c->n++;
    if (c->n < CONFIG_RFS_BASELINE_WARMUP) return 0;
    if (hot) { if (c->hot < 255) c->hot++; } else c->hot = 0;
    if (!c->alerting && c->hot >= b->hot_needed) { c->alerting = true; return +1; }
    if (c->alerting && c->hot == 0) { c->alerting = false; return -1; }
    return 0;
}
