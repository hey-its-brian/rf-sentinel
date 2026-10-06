#pragma once
#include <Arduino.h>

// Parses $RFS frames from the node into the Model. Call protoPoll() often.
void protoBegin();
void protoPoll();
void protoSend(const char* payload);   // e.g. "CMD,PING"
