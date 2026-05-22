#include "StatusBar.h"
#include "Theme.h"
#include <M5Unified.h>
#include <time.h>

namespace {
int clampBatteryLevel(int level) {
    if (level < 0) return 0;
    if (level > 100) return 100;
    return level;
}

int readBatteryLevel() {
    int level = M5.Power.getBatteryLevel();
    if (level >= 0) return clampBatteryLevel(level);

    int millivolts = M5.Power.getBatteryVoltage();
    if (millivolts > 0) {
        float pct = ((float)millivolts - 3300.0f) * 100.0f / (4150.0f - 3350.0f);
        return clampBatteryLevel((int)(pct + 0.5f));
    }

    return 0;
}

bool isChargingKnown() {
    return M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging;
}
}

void StatusBar::render(M5Canvas& canvas) {
    // Dark panel background
    canvas.fillRect(0, 0, Theme::SCREEN_W, Theme::STATUS_BAR_H, Theme::BAR_BG);
    canvas.drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::BORDER);

    canvas.setTextSize(Theme::FONT_SIZE);

    // Left side: battery icon + %, plus clock when time is known.
    unsigned long now = millis();
    if (_smoothedBattery < 0 || now - _lastBatteryRead >= 2000) {
        int raw = readBatteryLevel();
        if (_smoothedBattery < 0) {
            _smoothedBattery = (float)raw;
        } else {
            _smoothedBattery = _smoothedBattery * 0.85f + (float)raw * 0.15f;
        }
        _lastBatteryRead = now;
    }
    int batLevel = clampBatteryLevel((int)(_smoothedBattery + 0.5f));
    bool charging = isChargingKnown();

    // Battery icon: always shown (compact)
    int bx = 2, by = 2, bw = 14, bh = 7;
    uint16_t batColor = Theme::PRIMARY;
    if (batLevel <= 10) batColor = Theme::ERROR;
    else if (batLevel <= 30) batColor = Theme::WARNING;
    canvas.drawRect(bx, by, bw, bh, batColor);
    canvas.fillRect(bx + bw, by + 2, 2, 3, batColor);
    int fillW = (bw - 2) * batLevel / 100;
    if (fillW > 0) canvas.fillRect(bx + 1, by + 1, fillW, bh - 2, batColor);
    if (charging) {
        canvas.drawLine(bx + 7, by + 1, bx + 5, by + 5, Theme::BG);
        canvas.drawLine(bx + 5, by + 5, bx + 8, by + 5, Theme::BG);
        canvas.drawLine(bx + 8, by + 5, bx + 6, by + 8, Theme::BG);
    }

    int textX = bx + bw + 4;

    char batStr[6];
    snprintf(batStr, sizeof(batStr), "%d%%", batLevel);
    canvas.setTextColor(charging ? Theme::ACCENT : batColor);
    canvas.setCursor(textX, Theme::STATUS_PAD);
    canvas.print(batStr);
    textX += strlen(batStr) * Theme::CHAR_W + 6;

    // Show clock too if system time is valid (GPS or NTP).
    time_t t = time(nullptr);
    if (t > 1700000000) {
        struct tm* tm = localtime(&t);
        char clockStr[8];
        snprintf(clockStr, sizeof(clockStr), "%d:%02d", tm->tm_hour, tm->tm_min);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(textX, Theme::STATUS_PAD);
        canvas.print(clockStr);
    }

    // Center text — flash "ANNOUNCED" briefly, otherwise show mode
    const char* centerText = _transportMode;
    uint16_t centerColor = Theme::ACCENT;
    if (millis() < _announceFlashUntil) {
        centerText = "ANNOUNCED";
        centerColor = Theme::PRIMARY;
    }
    canvas.setTextColor(centerColor);
    int modeLen = strlen(centerText) * Theme::CHAR_W;
    canvas.setCursor((Theme::SCREEN_W - modeLen) / 2, Theme::STATUS_PAD);
    canvas.print(centerText);

    // Connection indicators (right) — LoRa + TCP + AutoIface peers
    int rx = Theme::SCREEN_W - Theme::STATUS_PAD;

    // TCP indicator
    if (_tcpConnected) {
        const char* tcpStr = "TCP";
        int tcpW = strlen(tcpStr) * Theme::CHAR_W;
        rx -= tcpW;
        canvas.setTextColor(Theme::ACCENT);
        canvas.setCursor(rx, Theme::STATUS_PAD);
        canvas.print(tcpStr);
        rx -= 4;
    }

    // AutoInterface peers indicator (only shown when at least one peer)
    if (_autoIfacePeers > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "L:%d", _autoIfacePeers);
        int w = strlen(buf) * Theme::CHAR_W;
        rx -= w;
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(rx, Theme::STATUS_PAD);
        canvas.print(buf);
        rx -= 4;
    }

    // LoRa indicator
    const char* loraStr = _loraOnline ? "LoRa" : "----";
    uint16_t loraColor = _loraOnline ? Theme::PRIMARY : Theme::MUTED;
    int loraW = strlen(loraStr) * Theme::CHAR_W;
    rx -= loraW;
    canvas.fillCircle(rx - 4, Theme::STATUS_PAD + 3, 2, loraColor);
    canvas.setTextColor(loraColor);
    canvas.setCursor(rx, Theme::STATUS_PAD);
    canvas.print(loraStr);
}
