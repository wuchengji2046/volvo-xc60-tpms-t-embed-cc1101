#pragma once
#include <Arduino.h>

struct WheelState {
  const char* pos = "--";
  const char* id = "00000000";
  bool seen = false;
  float pressure_kPa = NAN;
  float pressure_bar = NAN;
  float temperature_C = NAN;
  int rssi = 0;
  uint32_t lastSeenMs = 0;
  uint32_t hitCount = 0;
  uint32_t status = 0;
  char flags[8] = "";
};
