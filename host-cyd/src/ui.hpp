#pragma once
#include <Arduino.h>

enum Page : uint8_t { PG_HOME, PG_SUB, PG_NRF, PG_GPS, PG_WIFI, PG_BLE, PG_SET, PG_COUNT };

void uiBegin();
void uiTick();         // redraw as needed, handle touch
void uiNodeLinked();   // call once when the node link comes up: pushes saved settings to it
