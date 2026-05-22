#include "HomeScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"
#include <algorithm>

struct RadioPresetHome {
    const char* name;
    uint8_t sf; uint32_t bw; uint8_t cr; int8_t txPower;
};
static const RadioPresetHome HOME_PRESETS[] = {
    {"Short Turbo",   7,  500000, 5,  14},
    {"Short Fast",    7,  250000, 5,  14},
    {"Short Slow",    8,  250000, 5,  14},
    {"Medium Fast",   9,  250000, 5,  17},
    {"Medium Slow",   10, 250000, 5,  17},
    {"Long Turbo",    11, 500000, 8,  LORA_MAX_TX_POWER},
    {"Long Fast",     11, 250000, 5,  LORA_MAX_TX_POWER},
    {"Long Moderate", 11, 125000, 8,  LORA_MAX_TX_POWER},
};
static const char* detectPresetName(const UserSettings& s) {
    for (int i = 0; i < 8; i++) {
        if (s.loraSF == HOME_PRESETS[i].sf && s.loraBW == HOME_PRESETS[i].bw
            && s.loraCR == HOME_PRESETS[i].cr && s.loraTxPower == HOME_PRESETS[i].txPower)
            return HOME_PRESETS[i].name;
    }
    return "Custom";
}

static void drawFitted(M5Canvas& canvas, const char* text, int x, int y, int maxW) {
    if (canvas.textWidth(text) <= maxW) {
        canvas.drawString(text, x, y);
        return;
    }
    char buf[64];
    int len = std::min((int)strlen(text), (int)sizeof(buf) - 3);
    while (len > 0) {
        memcpy(buf, text, len);
        buf[len] = '.';
        buf[len + 1] = '.';
        buf[len + 2] = '\0';
        if (canvas.textWidth(buf) <= maxW) {
            canvas.drawString(buf, x, y);
            return;
        }
        len--;
    }
}

bool HomeScreen::handleKey(const KeyEvent& event) {
    if (event.enter) {
        if (_announceCb) _announceCb();
        _announceFlashUntil = millis() + 1500;
        return true;
    }
    return false;
}

void HomeScreen::render(M5Canvas& canvas) {
    int y = Theme::CONTENT_Y + 3;
    const int pad = 8;
    const int cardX = 3;
    const int cardW = Theme::SCREEN_W - 6;

    int cardY = y;
    int cardH = 35;
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 4, Theme::BG_ELEVATED);
    canvas.drawRoundRect(cardX, cardY, cardW, cardH, 4, Theme::BORDER);
    canvas.fillRoundRect(cardX + 3, cardY + 4, 3, cardH - 8, 1, Theme::ACCENT);

    Theme::useUiFont(canvas);
    canvas.setTextColor(Theme::TEXT_PRIMARY);
    if (_userConfig && !_userConfig->settings().displayName.isEmpty()) {
        drawFitted(canvas, _userConfig->settings().displayName.c_str(),
                   cardX + pad, cardY + 2, cardW - pad * 2);
    } else {
        canvas.setTextColor(Theme::MUTED);
        canvas.drawString("(no name set)", cardX + pad, cardY + 2);
    }

    Theme::useSmallFont(canvas);
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setCursor(cardX + pad, cardY + 22);
    canvas.print("LXMF ");
    canvas.setTextColor(Theme::ACCENT);
    if (_rns) {
        canvas.print(_rns->destinationHashStr());
    }

    y = cardY + cardH + 4;
    cardY = y;
    cardH = 34;
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 4, Theme::BG_ELEVATED);
    canvas.drawRoundRect(cardX, cardY, cardW, cardH, 4, Theme::BORDER);
    canvas.fillRoundRect(cardX + 3, cardY + 4, 3, cardH - 8, 1, Theme::PRIMARY);

    if (_radio && _radio->isRadioOnline()) {
        if (_userConfig) {
            const char* preset = detectPresetName(_userConfig->settings());
            Theme::useUiFont(canvas);
            canvas.setTextColor(Theme::TEXT_PRIMARY);
            char headline[48];
            snprintf(headline, sizeof(headline), "%s  SF%d  %luk",
                preset,
                _radio->getSpreadingFactor(),
                (unsigned long)(_radio->getSignalBandwidth() / 1000));
            drawFitted(canvas, headline, cardX + pad, cardY + 2, cardW - pad * 2);
        }
    } else {
        Theme::useUiFont(canvas);
        canvas.setTextColor(Theme::MUTED);
        canvas.drawString("LoRa Offline", cardX + pad, cardY + 2);
    }

    if (_radio && _radio->isRadioOnline()) {
        Theme::useSmallFont(canvas);
        canvas.setTextColor(Theme::MUTED);
        canvas.setCursor(cardX + pad, cardY + 22);
        canvas.printf("%.1f MHz  TX:%d dBm  CR:%d  P:%d L:%d",
            _radio->getFrequency() / 1000000.0,
            _radio->getTxPower(), _radio->getCodingRate4(),
            _rns ? (int)_rns->pathCount() : 0,
            _rns ? (int)_rns->linkCount() : 0);
    }

    y = cardY + cardH + 4;
    {
        const char* label = "Announce";
        Theme::useUiFont(canvas);
        int btnW = canvas.textWidth(label) + 22;
        int btnX = (Theme::SCREEN_W - btnW) / 2;
        int btnH = 18;
        int btnY = std::min(y, Theme::CONTENT_Y + Theme::CONTENT_H - btnH - 2);
        canvas.fillRoundRect(btnX, btnY, btnW, btnH, 4, Theme::SELECTION_BG);
        canvas.drawRoundRect(btnX, btnY, btnW, btnH, 4, Theme::PRIMARY);
        canvas.setTextColor(Theme::TEXT_PRIMARY);
        canvas.setCursor(btnX + 11, btnY + 1);
        canvas.print(label);
    }

    // Announce flash toast
    if (millis() < _announceFlashUntil) {
        const char* msg = "Announced!";
        Theme::useSmallFont(canvas);
        int tw = strlen(msg) * Theme::CHAR_W + 12;
        int th = Theme::CHAR_H + 8;
        int tx = (Theme::CONTENT_W - tw) / 2;
        int ty = Theme::CONTENT_Y + Theme::CONTENT_H - th - 4;
        canvas.fillRoundRect(tx, ty, tw, th, 3, Theme::SELECTION_BG);
        canvas.drawRoundRect(tx, ty, tw, th, 3, Theme::PRIMARY);
        canvas.setTextColor(Theme::TEXT_PRIMARY);
        canvas.setCursor(tx + 6, ty + 4);
        canvas.print(msg);
    }
    Theme::useSmallFont(canvas);
}
