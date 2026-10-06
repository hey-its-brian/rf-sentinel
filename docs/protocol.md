# RF Sentinel host protocol v1

Text frames, one per line, NMEA style. Same stream on the host UART (115200 8N1) and on the C5's USB console.

```
$RFS,<TYPE>,<fields...>*<XOR checksum of everything between $ and *, 2 hex digits>\n
```

## Node to host

| Type | Fields | Rate |
|---|---|---|
| `HELLO` | fw_version, proto_version, `cc=0/1`, `nrf=0/1`, `lora=0/1`, `gps=1`, `wifi=0/1` | at boot and on `CMD,HELLO` |
| `STAT` | uptime_s, cc_ok, nrf_ok, lora_ok, gps_alive, free_heap | 1 s |
| `GPS` | fix, sats_used, sats_in_view, hdop, lat, lon, alt_m, cn0_avg, cn0_max, utc | 1 s |
| `SUB` | 21, then 21 RSSI values in dBm for 433.0, 433.1 ... 435.0 MHz | every sweep (250 ms) |
| `NRF` | 126, then a 126-character hex string. Digit i = how many of the last 8 sweeps saw a carrier on 2400+i MHz (0..8) | every 8 sweeps (about 2 s) |
| `LORA` | packets_since_last, rssi, snr | when packets arrive (only with `RFS_LORA`) |
| `WIFI` | total_aps, aps_2g4, aps_5g, strongest_rssi | every 15 s |
| `WAP` | bssid_hex, channel, rssi, ssid | up to 8 after each `WIFI` |
| `EVT` | level (`info`/`warn`), source (`sub`/`nrf`/`gps`/`wifi`), text | on change |
| `PONG` | node_ms | reply to `CMD,PING` |
| `ERR` | text | on bad command |

## Host to node

| Command | Effect |
|---|---|
| `$RFS,CMD,PING*xx` | replies `PONG` |
| `$RFS,CMD,HELLO*xx` | replies `HELLO` |
| `$RFS,CMD,QUIET*xx` | stop streaming `SUB`/`NRF` rows (events and status continue) |
| `$RFS,CMD,STREAM*xx` | resume streaming |
| `$RFS,CMD,RATE,<ms>*xx` | sweep period, 100 to 5000 ms |

The checksum on commands is optional: send `$RFS,CMD,PING` with no `*xx` and it is accepted.

## Alerts

Each channel keeps a slow EWMA mean and variance. A sample counts as "hot" when it exceeds the mean by more than max(min_delta, 3 sigma). A few consecutive hot samples raise a `warn` event; the channel returns to `info ... quiet` when it calms down. Nothing fires before `RFS_BASELINE_WARMUP` samples (default 80, about 20 s for sub-GHz, about 3 min for the nRF rows). GPS feeds negated satellite count and mean C/N0 into the same logic so sudden drops raise `warn,gps,...` events.
