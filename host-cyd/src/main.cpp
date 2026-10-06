// RF Sentinel host for the 2.8" CYD. The C5 node does the radio work and
// streams $RFS frames over UART2; this board draws them and adds its own
// Wi-Fi and BLE view of the room.
#include <Arduino.h>
#include "board.h"
#include "model.hpp"
#include "proto.hpp"
#include "scanners.hpp"
#include "ui.hpp"

Model model;

static void ledInit() {
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  digitalWrite(PIN_LED_R, HIGH); digitalWrite(PIN_LED_G, HIGH); digitalWrite(PIN_LED_B, HIGH);  // active low, off
}
static void ledStatus() {
  bool warnRecent = model.evtCount && model.evt[(model.evtHead - 1 + EVT_MAX) % EVT_MAX].warn &&
                    millis() - model.evt[(model.evtHead - 1 + EVT_MAX) % EVT_MAX].ms < 10000;
  digitalWrite(PIN_LED_R, warnRecent ? LOW : HIGH);
  digitalWrite(PIN_LED_G, model.linkAlive() && !warnRecent ? LOW : HIGH);
  digitalWrite(PIN_LED_B, HIGH);
}

void setup() {
  Serial.begin(115200);
  ledInit();
  uiBegin();
  protoBegin();
  scannersBegin();
  protoSend("CMD,HELLO");
  Serial.println("[cyd] rf-sentinel host up");
}

void loop() {
  protoPoll();
  scannersPoll();
  uiTick();
  ledStatus();
  static bool wasLinked = false;
  bool linked = model.linkAlive();
  if (linked && !wasLinked) uiNodeLinked();
  wasLinked = linked;
  static uint32_t lastHello = 0;
  if (!model.linkAlive() && millis() - lastHello > 3000) { lastHello = millis(); protoSend("CMD,HELLO"); }
  delay(5);
}
