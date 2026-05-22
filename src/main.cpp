#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <Preferences.h>
#include <math.h>
#include <rtl_433_ESP.h>

#include "board_pins.h"
#include "xc60_tpms_config.h"
#include "wheel_state.h"
#include "display_layout.h"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "TEmbedCC1101-DEV"
#endif

#if defined(XC60_FORCE_RX_GDO2)
static constexpr int RTL_RECEIVE_PIN = PIN_CC1101_GDO2;
#else
static constexpr int RTL_RECEIVE_PIN = PIN_CC1101_GDO0;
#endif

TFT_eSPI tft;
rtl_433_ESP rtlReceiver;

static WheelState wheels[4];
static Preferences prefs;
static bool prefsReady = false;
static bool receivedThisBoot[4] = {false, false, false, false};
static float lastCachedPressureBar[4] = {NAN, NAN, NAN, NAN};
static float lastCachedTempC[4] = {NAN, NAN, NAN, NAN};
static uint32_t lastCacheSaveMs[4] = {0, 0, 0, 0};

static uint32_t totalCount = 0;
static uint32_t modelCount = 0;
static uint32_t mineCount = 0;
static uint32_t unknownAbarthCount = 0;
static uint32_t jsonErrCount = 0;
static uint32_t pendingOverflowCount = 0;
static int lastRSSI = 0;
static String lastEvent = "boot";
static bool displayOk = false;
static bool rfStarted = false;
static bool debugPage = false;
static uint32_t lastDisplayMs = 0;
static uint32_t lastHeartbeatMs = 0;
static uint32_t lastButtonMs = 0;

static char rtlMessageBuffer[RTL_JSON_BUFFER_SIZE];
static char pendingJson[RTL_JSON_BUFFER_SIZE];
static volatile bool pendingJsonReady = false;
static portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;

static String idToLower8(const String& raw) {
  String s = raw;
  s.trim();
  s.toLowerCase();
  if (s.startsWith("0x")) s = s.substring(2);
  while (s.length() < 8) s = "0" + s;
  if (s.length() > 8) s = s.substring(s.length() - 8);
  return s;
}

static String jsonIdToString(JsonVariantConst v) {
  if (v.is<const char*>()) return idToLower8(String(v.as<const char*>()));
  if (v.is<String>()) return idToLower8(v.as<String>());
  if (v.is<unsigned long>()) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%08lx", static_cast<unsigned long>(v.as<unsigned long>()));
    return String(buf);
  }
  if (v.is<uint32_t>()) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%08lx", static_cast<unsigned long>(v.as<uint32_t>()));
    return String(buf);
  }
  if (v.is<int>()) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%08lx", static_cast<unsigned long>(static_cast<uint32_t>(v.as<int>())));
    return String(buf);
  }
  return "";
}

static int findWheelById(const String& id) {
  for (int i = 0; i < 4; ++i) {
    if (id.equalsIgnoreCase(wheels[i].id)) return i;
  }
  return -1;
}

static bool modelIsAbarth(const String& model) {
  String m = model;
  m.toLowerCase();

  String target = String(TARGET_MODEL);
  target.toLowerCase();

  return (m.indexOf("abarth") >= 0) || (m == target);
}

static float readFloatWithFallback(JsonDocument& doc, const char* a, const char* b = nullptr, const char* c = nullptr) {
  if (doc[a].is<float>() || doc[a].is<double>() || doc[a].is<int>()) return doc[a].as<float>();
  if (b && (doc[b].is<float>() || doc[b].is<double>() || doc[b].is<int>())) return doc[b].as<float>();
  if (c && (doc[c].is<float>() || doc[c].is<double>() || doc[c].is<int>())) return doc[c].as<float>();
  return NAN;
}

static int readIntWithFallback(JsonDocument& doc, const char* a, const char* b = nullptr, int fallback = 0) {
  if (doc[a].is<int>()) return doc[a].as<int>();
  if (b && doc[b].is<int>()) return doc[b].as<int>();
  return fallback;
}

static void initWheelStates() {
  for (int i = 0; i < 4; ++i) {
    wheels[i].pos = WHEEL_CONFIGS[i].pos;
    wheels[i].id = WHEEL_CONFIGS[i].id;
    wheels[i].seen = false;
    wheels[i].pressure_kPa = NAN;
    wheels[i].pressure_bar = NAN;
    wheels[i].temperature_C = NAN;
    wheels[i].rssi = 0;
    wheels[i].lastSeenMs = 0;
    wheels[i].hitCount = 0;
    wheels[i].status = 0;
    wheels[i].flags[0] = '\0';
  }
}

static void initPowerAndRfPath() {
  pinMode(PIN_BOARD_PWR_EN, OUTPUT);
  digitalWrite(PIN_BOARD_PWR_EN, LOW);
  delay(200);
  digitalWrite(PIN_BOARD_PWR_EN, HIGH);
  delay(500);

  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_CC1101_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  digitalWrite(PIN_CC1101_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);

  pinMode(PIN_RF_SW1, OUTPUT);
  pinMode(PIN_RF_SW0, OUTPUT);
  digitalWrite(PIN_RF_SW1, HIGH);
  digitalWrite(PIN_RF_SW0, HIGH);  // SW1=1, SW0=1: 434 MHz path.

  pinMode(PIN_BOARD_USER_KEY, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
}

static void printBootBanner() {
  Serial.println();
  Serial.println("==== XC60 TPMS T-Embed-CC1101 ====");
  Serial.printf("Version: %s\n", PROJECT_VERSION);
  Serial.println("Board: LILYGO T-Embed-CC1101 / ESP32-S3");
  Serial.println("Goal: car test build for TPMS signal receive, ID decode, pressure/temp display");
  Serial.printf("Display: ST7789 320x170 CS=%d DC=%d RST=%d BL=%d SCK=%d MOSI=%d MISO=%d\n",
                PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL, PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO);
  Serial.printf("RF: CC1101 %.2fMHz FSK\n", TPMS_FREQUENCY_MHZ);
  Serial.println("Decoder target: Abarth-124Spider TPMS");
  Serial.println("Mode: FSK_PULSE_PCM / OOK_MODULATION=false");
  Serial.printf("IDs: LF=%s RF=%s LR=%s RR=%s\n",
                WHEEL_CONFIGS[0].id, WHEEL_CONFIGS[1].id, WHEEL_CONFIGS[2].id, WHEEL_CONFIGS[3].id);
  Serial.printf("Power: BOARD_PWR_EN=%d\n", PIN_BOARD_PWR_EN);
  Serial.printf("RF pins: CS=%d GDO0=%d GDO2=%d RX=%d SCK=%d MISO=%d MOSI=%d SW1=%d SW0=%d\n",
                PIN_CC1101_CS, PIN_CC1101_GDO0, PIN_CC1101_GDO2, RTL_RECEIVE_PIN,
                PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RF_SW1, PIN_RF_SW0);
}

static void initDisplay() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  displayOk = true;
}

static void makeCacheKey(char* out, size_t outSize, int idx, const char* suffix) {
  snprintf(out, outSize, "w%d_%s", idx, suffix);
}

static void initWheelCache() {
  prefsReady = prefs.begin("xc60tpms", false);
  if (!prefsReady) {
    Serial.println("[CACHE] Preferences open failed; startup cache disabled");
    return;
  }

  for (int i = 0; i < 4; ++i) {
    char keyValid[12];
    char keyBar[12];
    char keyTemp[12];
    makeCacheKey(keyValid, sizeof(keyValid), i, "valid");
    makeCacheKey(keyBar, sizeof(keyBar), i, "bar");
    makeCacheKey(keyTemp, sizeof(keyTemp), i, "temp");

    const bool valid = prefs.getBool(keyValid, false);
    const float cachedBar = prefs.getFloat(keyBar, NAN);
    const float cachedTemp = prefs.getFloat(keyTemp, NAN);

    if (valid && !isnan(cachedBar) && !isnan(cachedTemp)) {
      WheelState& w = wheels[i];
      w.seen = true;
      w.pressure_bar = cachedBar;
      w.pressure_kPa = cachedBar * 100.0f;
      w.temperature_C = cachedTemp;
      w.rssi = 0;
      w.lastSeenMs = 0;
      lastCachedPressureBar[i] = cachedBar;
      lastCachedTempC[i] = cachedTemp;
      Serial.printf("[CACHE] loaded pos=%s pressure=%.2fbar temp=%.0fC\n", w.pos, w.pressure_bar, w.temperature_C);
    }
  }
}

static void saveWheelCacheIfNeeded(int idx) {
  if (!prefsReady || idx < 0 || idx >= 4) return;

  WheelState& w = wheels[idx];
  if (!w.seen || isnan(w.pressure_bar) || isnan(w.temperature_C)) return;

  const uint32_t now = millis();
  const bool firstSaveThisBoot = (lastCacheSaveMs[idx] == 0);
  const bool saveIntervalElapsed = (now - lastCacheSaveMs[idx] >= CACHE_SAVE_MIN_INTERVAL_MS);
  const bool pressureChanged = isnan(lastCachedPressureBar[idx]) || fabsf(w.pressure_bar - lastCachedPressureBar[idx]) >= 0.01f;
  const bool tempChanged = isnan(lastCachedTempC[idx]) || fabsf(w.temperature_C - lastCachedTempC[idx]) >= 1.0f;

  if (!firstSaveThisBoot && !saveIntervalElapsed && !pressureChanged && !tempChanged) return;

  char keyValid[12];
  char keyBar[12];
  char keyTemp[12];
  makeCacheKey(keyValid, sizeof(keyValid), idx, "valid");
  makeCacheKey(keyBar, sizeof(keyBar), idx, "bar");
  makeCacheKey(keyTemp, sizeof(keyTemp), idx, "temp");

  prefs.putBool(keyValid, true);
  prefs.putFloat(keyBar, w.pressure_bar);
  prefs.putFloat(keyTemp, w.temperature_C);
  lastCachedPressureBar[idx] = w.pressure_bar;
  lastCachedTempC[idx] = w.temperature_C;
  lastCacheSaveMs[idx] = now;
  Serial.printf("[CACHE] saved pos=%s pressure=%.2fbar temp=%.0fC\n", w.pos, w.pressure_bar, w.temperature_C);
}

static void drawTemperatureValue(int rightX, int y, const char* digits, uint16_t color) {
  // Smaller right-aligned temperature display.
  // The caller passes the desired right edge, so the whole value + °C moves together.
  // Keep manual degree mark to avoid Unicode font dependency.
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextFont(2);

  const int digitsW = tft.textWidth(digits, 2);
  const int cW = tft.textWidth("C", 2);
  constexpr int degreeRadius = 2;
  constexpr int gap = 2;
  const int totalW = digitsW + gap + degreeRadius * 2 + 2 + cW;
  const int startX = rightX - totalW;

  tft.drawString(digits, startX, y);
  const int degreeCenterX = startX + digitsW + gap + degreeRadius;
  const int degreeCenterY = y + 3;
  tft.drawCircle(degreeCenterX, degreeCenterY, degreeRadius, color);
  // Use the same top Y as the temperature number, so number and unit are aligned.
  tft.drawString("C", degreeCenterX + degreeRadius + 2, y);
}

static void drawCardFrame(int x, int y, int w, int h, int r, uint16_t color) {
  // Some ST7789/TFT_eSPI combinations show the corner arcs from drawRoundRect()
  // but do not visibly render the straight edge segments as expected.
  // Draw the straight segments explicitly, then keep drawRoundRect() for the arcs.
  if (w <= 0 || h <= 0) return;

  const int safeR = max(0, min(r, min(w, h) / 2));
  const int horizontalLen = max(0, w - 2 * safeR);
  const int verticalLen = max(0, h - 2 * safeR);

  if (horizontalLen > 0) {
    tft.drawFastHLine(x + safeR, y, horizontalLen, color);
    tft.drawFastHLine(x + safeR, y + h - 1, horizontalLen, color);
  }
  if (verticalLen > 0) {
    tft.drawFastVLine(x, y + safeR, verticalLen, color);
    tft.drawFastVLine(x + w - 1, y + safeR, verticalLen, color);
  }

  tft.drawRoundRect(x, y, w, h, safeR, color);
}

static void drawWheelCard(int idx, int x, int y) {
  WheelState& w = wheels[idx];
  const bool hasValue = w.seen && !isnan(w.pressure_bar) && !isnan(w.temperature_C);
  const uint32_t age = receivedThisBoot[idx] ? (millis() - w.lastSeenMs) : UINT32_MAX;

  uint16_t border = TFT_DARKGREY;
  uint16_t labelColor = TFT_DARKGREY;
  uint16_t valueColor = TFT_DARKGREY;
  const char* state = "WT";

  if (!receivedThisBoot[idx]) {
    state = "WT";
    if (hasValue) {
      border = TFT_YELLOW;
      labelColor = TFT_WHITE;
      valueColor = TFT_WHITE;
    }
  } else if (age > WHEEL_OLD_MS) {
    border = TFT_ORANGE;
    labelColor = TFT_ORANGE;
    valueColor = TFT_ORANGE;
    state = "OD";
  } else {
    border = TFT_GREEN;
    labelColor = TFT_WHITE;
    valueColor = TFT_WHITE;
    state = "OK";
  }

  drawCardFrame(x, y, CARD_W, CARD_H, 8, border);

  tft.setTextFont(2);
  tft.setTextColor(labelColor, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(state, x + CARD_W - 7, y + 4);

  char pressure[16];
  if (hasValue) snprintf(pressure, sizeof(pressure), "%.2f", w.pressure_bar);
  else snprintf(pressure, sizeof(pressure), "--.--");

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(6);
  tft.setTextColor(valueColor, TFT_BLACK);
  tft.drawString(pressure, x + CARD_W / 2 - 12, y + 40);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(valueColor, TFT_BLACK);

  char tempDigits[12];
  if (hasValue) snprintf(tempDigits, sizeof(tempDigits), "%.0f", w.temperature_C);
  else snprintf(tempDigits, sizeof(tempDigits), "--");
  drawTemperatureValue(x + CARD_W - 8, y + CARD_H - 20, tempDigits, valueColor);
}


static void drawMainScreen() {
  if (!displayOk) return;
  tft.fillScreen(TFT_BLACK);
  drawWheelCard(0, LEFT_X, TOP_Y);
  drawWheelCard(1, RIGHT_X, TOP_Y);
  drawWheelCard(2, LEFT_X, BOTTOM_Y);
  drawWheelCard(3, RIGHT_X, BOTTOM_Y);
}

static void drawDebugScreen() {
  if (!displayOk) return;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("DEBUG - T-Embed CC1101", 6, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char line[96];
  snprintf(line, sizeof(line), "RF %.2fMHz FSK RX:%d", TPMS_FREQUENCY_MHZ, RTL_RECEIVE_PIN);
  tft.drawString(line, 6, 28);
  snprintf(line, sizeof(line), "CS:%d GDO0:%d GDO2:%d", PIN_CC1101_CS, PIN_CC1101_GDO0, PIN_CC1101_GDO2);
  tft.drawString(line, 6, 48);
  snprintf(line, sizeof(line), "SPI SCK:%d MISO:%d MOSI:%d", PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  tft.drawString(line, 6, 68);
  snprintf(line, sizeof(line), "T:%lu A:%lu M:%lu U:%lu J:%lu", static_cast<unsigned long>(totalCount), static_cast<unsigned long>(modelCount), static_cast<unsigned long>(mineCount), static_cast<unsigned long>(unknownAbarthCount), static_cast<unsigned long>(jsonErrCount));
  tft.drawString(line, 6, 88);
  snprintf(line, sizeof(line), "RTL msg:%d RSSI:%d AVG:%d TH:%d", rtl_433_ESP::messageCount, rtl_433_ESP::currentRssi, rtl_433_ESP::averageRssi, rtl_433_ESP::rssiThreshold);
  tft.drawString(line, 6, 108);
  snprintf(line, sizeof(line), "Sig:%d Ign:%d Unp:%d Ovf:%lu", rtl_433_ESP::totalSignals, rtl_433_ESP::ignoredSignals, rtl_433_ESP::unparsedSignals, static_cast<unsigned long>(pendingOverflowCount));
  tft.drawString(line, 6, 128);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Press KEY/BOOT to toggle", 6, 150);
}

static void rtlCallback(char* message) {
  if (message == nullptr) return;
  portENTER_CRITICAL(&pendingMux);
  if (pendingJsonReady) {
    pendingOverflowCount++;
  } else {
    strlcpy(pendingJson, message, RTL_JSON_BUFFER_SIZE);
    pendingJsonReady = true;
  }
  portEXIT_CRITICAL(&pendingMux);
}

static bool fetchPendingJson(char* out, size_t outSize) {
  bool hasMessage = false;
  portENTER_CRITICAL(&pendingMux);
  if (pendingJsonReady) {
    strlcpy(out, pendingJson, outSize);
    pendingJsonReady = false;
    hasMessage = true;
  }
  portEXIT_CRITICAL(&pendingMux);
  return hasMessage;
}

static void processRtl433Json(const char* json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    jsonErrCount++;
    lastEvent = "json-err";
    Serial.printf("[JSON-ERR] reason=%s raw=%s\n", err.c_str(), json);
    return;
  }

  String model = doc["model"].is<const char*>() ? String(doc["model"].as<const char*>()) : "";
  String protocol = doc["protocol"].is<const char*>() ? String(doc["protocol"].as<const char*>()) : "";

  // rtl_433_ESP status messages are useful but are not TPMS data.
  // Do not count them as received TPMS messages, otherwise total/model/mine becomes misleading.
  if (model == "status" || protocol.indexOf("status") >= 0) {
    lastRSSI = readIntWithFallback(doc, "signalRssi", "RTLRssi", lastRSSI);
    lastEvent = "status";
    return;
  }

  totalCount++;

  if (!modelIsAbarth(model)) {
    lastRSSI = readIntWithFallback(doc, "rssi", "RSSI", lastRSSI);
    lastEvent = "other";
    Serial.printf("[OTHER] model=%s raw=%s\n", model.c_str(), json);
    return;
  }

  modelCount++;
  String id = jsonIdToString(doc["id"]);
  if (id.length() == 0) {
    jsonErrCount++;
    lastEvent = "missing-id";
    Serial.printf("[JSON-ERR] reason=missing-id raw=%s\n", json);
    return;
  }

  float pressureKPa = readFloatWithFallback(doc, "pressure_kPa", "pressure_kpa", "pressure");
  float tempC = readFloatWithFallback(doc, "temperature_C", "temperature_c", "temperature");
  int rssi = readIntWithFallback(doc, "rssi", "RSSI", rtl_433_ESP::signalRssi);
  uint32_t status = doc["status"].is<uint32_t>() ? doc["status"].as<uint32_t>() : 0;
  const char* flagsText = doc["flags"].is<const char*>() ? doc["flags"].as<const char*>() : "";

  int idx = findWheelById(id);
  if (idx < 0) {
    unknownAbarthCount++;
    lastRSSI = rssi;
    lastEvent = "abarth-other";
    Serial.printf("[ABARTH-OTHER] id=%s pressure=%.2fbar temp=%.0fC rssi=%d raw=%s\n",
                  id.c_str(), isnan(pressureKPa) ? NAN : pressureKPa / 100.0f, tempC, rssi, json);
    return;
  }

  WheelState& w = wheels[idx];
  w.seen = true;
  w.pressure_kPa = pressureKPa;
  w.pressure_bar = isnan(pressureKPa) ? NAN : pressureKPa / 100.0f;
  w.temperature_C = tempC;
  w.rssi = rssi;
  w.lastSeenMs = millis();
  w.hitCount++;
  w.status = status;
  strlcpy(w.flags, flagsText, sizeof(w.flags));
  receivedThisBoot[idx] = true;
  saveWheelCacheIfNeeded(idx);

  mineCount++;
  lastRSSI = rssi;
  lastEvent = String("hit-") + w.pos;
  Serial.printf("[TPMS] pos=%s id=%s pressure=%.2fbar kPa=%.1f temp=%.0fC rssi=%d status=%lu flags=%s hit=%lu\n",
                w.pos, id.c_str(), w.pressure_bar, w.pressure_kPa, w.temperature_C, w.rssi,
                static_cast<unsigned long>(w.status), w.flags, static_cast<unsigned long>(w.hitCount));
}

static void printHeartbeat() {
  Serial.printf("[HB] total=%lu model=%lu mine=%lu unknown=%lu jsonErr=%lu lastRSSI=%d rf=%s display=%s last=%s rxGPIO=%d avgRSSI=%d threshold=%d ignored=%d unparsed=%d\n",
                static_cast<unsigned long>(totalCount), static_cast<unsigned long>(modelCount),
                static_cast<unsigned long>(mineCount), static_cast<unsigned long>(unknownAbarthCount),
                static_cast<unsigned long>(jsonErrCount), lastRSSI,
                rfStarted ? "ok" : "no", displayOk ? "ok" : "no", lastEvent.c_str(), RTL_RECEIVE_PIN,
                rtl_433_ESP::averageRssi, rtl_433_ESP::rssiThreshold,
                rtl_433_ESP::ignoredSignals, rtl_433_ESP::unparsedSignals);
}

static void handleButtons() {
  bool keyPressed = digitalRead(PIN_BOARD_USER_KEY) == LOW || digitalRead(PIN_ENCODER_BUTTON) == LOW;
  if (keyPressed && millis() - lastButtonMs > 350) {
    debugPage = !debugPage;
    lastButtonMs = millis();
    if (debugPage) drawDebugScreen(); else drawMainScreen();
  }
}

static void initRtl433() {
  memset(rtlMessageBuffer, 0, sizeof(rtlMessageBuffer));
  memset(pendingJson, 0, sizeof(pendingJson));
  rtlReceiver.setCallback(rtlCallback, rtlMessageBuffer, sizeof(rtlMessageBuffer));
  rtlReceiver.setRSSIThreshold(9);
  rtlReceiver.initReceiver(RTL_RECEIVE_PIN, TPMS_FREQUENCY_MHZ);
  rtlReceiver.enableReceiver();
  rfStarted = true;
  Serial.println("RF initialized: yes");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Log.begin(LOG_LEVEL_TRACE, &Serial);
  initWheelStates();
  initWheelCache();
  printBootBanner();
  initPowerAndRfPath();
  Serial.println("Power/RF path: BOARD_PWR_EN=HIGH, RF switch=434MHz, CS lines=HIGH");
  initDisplay();
  Serial.printf("Display initialized: %s\n", displayOk ? "yes" : "no");
  initRtl433();
  drawMainScreen();
  printHeartbeat();
}

void loop() {
  rtlReceiver.loop();
  handleButtons();

  char localJson[RTL_JSON_BUFFER_SIZE];
  if (fetchPendingJson(localJson, sizeof(localJson))) {
    processRtl433Json(localJson);
    if (!debugPage) drawMainScreen();
  }

  if (millis() - lastDisplayMs >= DISPLAY_REFRESH_MS) {
    lastDisplayMs = millis();
    if (debugPage) drawDebugScreen(); else drawMainScreen();
  }

  if (millis() - lastHeartbeatMs >= HEARTBEAT_MS) {
    lastHeartbeatMs = millis();
    printHeartbeat();
  }

  delay(1);
}
