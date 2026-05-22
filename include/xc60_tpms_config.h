#pragma once
#include <Arduino.h>

static constexpr float TPMS_FREQUENCY_MHZ = 433.92f;
static constexpr uint32_t DISPLAY_REFRESH_MS = 1000;
static constexpr uint32_t HEARTBEAT_MS = 5000;
static constexpr uint32_t RTL_STATUS_MS = 30000;

// Final UI status policy:
// WAIT = cached value shown after boot, but no fresh packet yet this boot.
// OK   = packet received this boot and still fresh.
// OLD  = packet received this boot, but no refresh for 5 minutes.
static constexpr uint32_t WHEEL_OLD_MS = 300000;  // 5 minutes

// Cache the latest valid values periodically so a power loss still leaves useful startup values.
static constexpr uint32_t CACHE_SAVE_MIN_INTERVAL_MS = 60000;  // 1 minute

static constexpr size_t RTL_JSON_BUFFER_SIZE = 2048;

struct WheelConfig {
  const char* pos;
  const char* id;
};

static constexpr WheelConfig WHEEL_CONFIGS[4] = {
  {"LF", "0dbc3751"},
  {"RF", "0e3f7905"},
  {"LR", "0dbc0ea4"},
  {"RR", "0dbc0ea6"},
};

static constexpr const char* TARGET_MODEL = "Abarth-124Spider";
