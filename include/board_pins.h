#pragma once

// LILYGO T-Embed-CC1101 pin map used by this project.
// The LILYGO examples name the CC1101 pins as LORA_* even though this board uses CC1101.

static constexpr int PIN_BOARD_USER_KEY = 6;
static constexpr int PIN_ENCODER_BUTTON = 0;
static constexpr int PIN_BOARD_PWR_EN   = 15;

// Shared SPI bus
static constexpr int PIN_SPI_SCK  = 11;
static constexpr int PIN_SPI_MOSI = 9;
static constexpr int PIN_SPI_MISO = 10;

// ST7789 TFT LCD, 1.9 inch 320x170
static constexpr int PIN_TFT_BL   = 21;
static constexpr int PIN_TFT_CS   = 41;
static constexpr int PIN_TFT_DC   = 16;
static constexpr int PIN_TFT_RST  = 40;

// CC1101 Sub-GHz radio
static constexpr int PIN_CC1101_CS   = 12;
static constexpr int PIN_CC1101_GDO0 = 3;
static constexpr int PIN_CC1101_GDO2 = 38;
static constexpr int PIN_RF_SW1      = 47;
static constexpr int PIN_RF_SW0      = 48;

// Optional peripherals, disabled but kept high to avoid SPI contention.
static constexpr int PIN_SD_CS = 13;

// WS2812 RGB ring, optional status output. Not used in this first test build.
static constexpr int PIN_WS2812 = 14;
