#pragma once

#include <M5GFX.h>

// =============================================================================
// RatCom — compact field-console theme
// =============================================================================

namespace Theme {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// --- Colors (RGB565) ---
constexpr uint16_t BG             = rgb565(5, 8, 10);
constexpr uint16_t BG_ELEVATED    = rgb565(12, 20, 24);
constexpr uint16_t BG_SURFACE     = rgb565(18, 29, 35);
constexpr uint16_t BG_HOVER       = rgb565(24, 48, 51);
constexpr uint16_t TEXT_PRIMARY   = rgb565(228, 243, 240);
constexpr uint16_t TEXT_SECONDARY = rgb565(145, 168, 165);
constexpr uint16_t TEXT_MUTED     = rgb565(82, 104, 102);
constexpr uint16_t PRIMARY        = rgb565(0, 224, 109);
constexpr uint16_t PRIMARY_MUTED  = rgb565(0, 168, 83);
constexpr uint16_t PRIMARY_SUBTLE = rgb565(6, 37, 26);
constexpr uint16_t SECONDARY      = TEXT_SECONDARY;
constexpr uint16_t MUTED          = TEXT_MUTED;
constexpr uint16_t SUCCESS        = rgb565(49, 233, 129);
constexpr uint16_t ERROR          = rgb565(255, 92, 108);
constexpr uint16_t WARNING        = rgb565(240, 192, 79);
constexpr uint16_t ACCENT         = rgb565(79, 215, 255);
constexpr uint16_t BORDER         = rgb565(33, 56, 59);
constexpr uint16_t DIVIDER        = rgb565(16, 35, 41);
constexpr uint16_t SELECTION_BG   = PRIMARY_SUBTLE;
constexpr uint16_t BAR_BG         = rgb565(7, 16, 20);
constexpr uint16_t TAB_ACTIVE     = TEXT_PRIMARY;
constexpr uint16_t TAB_INACTIVE   = TEXT_SECONDARY;
constexpr uint16_t BADGE_BG       = ERROR;
constexpr uint16_t BADGE_TEXT     = BG;

// --- Layout Metrics ---
constexpr int SCREEN_W       = 240;
constexpr int SCREEN_H       = 135;
constexpr int STATUS_BAR_H   = 18;
constexpr int TAB_BAR_H      = 22;
constexpr int CONTENT_Y      = STATUS_BAR_H;
constexpr int CONTENT_H      = SCREEN_H - STATUS_BAR_H - TAB_BAR_H;
constexpr int CONTENT_W      = SCREEN_W;

// --- Font ---
constexpr const lgfx::GFXfont* FONT_SMALL = nullptr;  // Built-in 6x8
constexpr int FONT_SIZE       = 1;
constexpr int CHAR_W          = 6;
constexpr int CHAR_H          = 8;
constexpr int UI_FONT_H       = 12;
constexpr int LIST_ROW_H      = 15;
constexpr int SHELL_TEXT_Y    = 2;

// --- Tab Bar ---
constexpr int TAB_COUNT       = 4;
constexpr int TAB_W           = SCREEN_W / TAB_COUNT;

// --- Status Bar ---
constexpr int STATUS_PAD      = 2;

inline void useSmallFont(M5Canvas& canvas) {
    canvas.setFont(nullptr);
    canvas.setTextSize(1);
}

inline void useUiFont(M5Canvas& canvas) {
    canvas.setFont(nullptr);
    canvas.setTextSize(1.5f);
}

}  // namespace Theme
