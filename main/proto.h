// RF Sentinel host protocol: NMEA-style text frames, one per line.
//   $RFS,<TYPE>,<field>,<field>,...*<XOR checksum, 2 hex>\n
// The checksum covers everything between '$' and '*'. Receivers that do not
// care about integrity can simply split on commas.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RFS_PROTO_VERSION "1"
#ifndef RFS_FW_VERSION
#define RFS_FW_VERSION "0.1.0"
#endif

void proto_init(void);
// Build a frame and write it to the host UART and the USB console.
void proto_send(const char *type, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
// Poll host UART for commands. Returns true and fills 'line' when a frame arrived.
bool proto_poll_cmd(char *line, size_t len);
// Verify a received $RFS frame; returns pointer to the payload after "$RFS," or NULL.
const char *proto_check(char *line);

#ifdef __cplusplus
}
#endif
