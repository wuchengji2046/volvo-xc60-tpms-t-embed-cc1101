#pragma once

// Final daily-use layout with a larger safe margin.
// This keeps all four cards inside the physical visible area of the T-Embed screen,
// especially the upper-left corner near the rounded bezel.
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 170;

static constexpr int OUTER_MARGIN_X = 8;
static constexpr int OUTER_MARGIN_Y = 8;
static constexpr int CARD_GAP_X = 4;
static constexpr int CARD_GAP_Y = 4;

// 8 + 150 + 4 + 150 + 8 = 320
// 8 + 75 + 4 + 75 + 8 = 170
static constexpr int CARD_W = 150;
static constexpr int CARD_H = 75;

static constexpr int LEFT_X = OUTER_MARGIN_X;
static constexpr int RIGHT_X = OUTER_MARGIN_X + CARD_W + CARD_GAP_X;
static constexpr int TOP_Y = OUTER_MARGIN_Y;
static constexpr int BOTTOM_Y = OUTER_MARGIN_Y + CARD_H + CARD_GAP_Y;
