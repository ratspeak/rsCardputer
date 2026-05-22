#include "DataCleanScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"

void DataCleanScreen::render(M5Canvas& canvas) {
    int cx = Theme::SCREEN_W / 2;

    // "RATSPEAK" title
    canvas.setTextSize(2);
    canvas.setTextColor(Theme::PRIMARY);
    const char* titleStr = "RATSPEAK";
    int tw = strlen(titleStr) * 12;
    canvas.setCursor(cx - tw / 2, Theme::CONTENT_Y + 4);
    canvas.print(titleStr);

    // "ratspeak.org" subtitle
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::MUTED);
    const char* sub = "ratspeak.org";
    int sw = strlen(sub) * Theme::CHAR_W;
    canvas.setCursor(cx - sw / 2, Theme::CONTENT_Y + 24);
    canvas.print(sub);

    // Message
    canvas.setTextColor(Theme::SECONDARY);
    const char* msg = "Old data found on SD card.";
    int mw = strlen(msg) * Theme::CHAR_W;
    canvas.setCursor(cx - mw / 2, Theme::CONTENT_Y + 40);
    canvas.print(msg);

    // Prompt
    const char* prompt = "Remove old data & start fresh?";
    int pw = strlen(prompt) * Theme::CHAR_W;
    canvas.setCursor(cx - pw / 2, Theme::CONTENT_Y + 52);
    canvas.print(prompt);

    if (_status) {
        // Show status message instead of buttons
        canvas.setTextColor(Theme::PRIMARY);
        int stw = strlen(_status) * Theme::CHAR_W;
        canvas.setCursor(cx - stw / 2, Theme::CONTENT_Y + 70);
        canvas.print(_status);
    } else {
        const char* yes = "Yes";
        const char* no  = "No";

        Theme::useUiFont(canvas);
        canvas.setTextColor(_selectedYes ? Theme::TEXT_PRIMARY : Theme::MUTED);
        int yw = canvas.textWidth(yes) + 20;
        int yesX = cx - 48 - yw / 2;
        canvas.fillRoundRect(yesX, Theme::CONTENT_Y + 70, yw, 18, 4,
                             _selectedYes ? Theme::SELECTION_BG : Theme::BG_ELEVATED);
        canvas.drawRoundRect(yesX, Theme::CONTENT_Y + 70, yw, 18, 4,
                             _selectedYes ? Theme::PRIMARY : Theme::BORDER);
        canvas.setCursor(yesX + 10, Theme::CONTENT_Y + 72);
        canvas.print(yes);

        canvas.setTextColor(!_selectedYes ? Theme::TEXT_PRIMARY : Theme::MUTED);
        int nw = canvas.textWidth(no) + 20;
        int noX = cx + 48 - nw / 2;
        canvas.fillRoundRect(noX, Theme::CONTENT_Y + 70, nw, 18, 4,
                             !_selectedYes ? Theme::SELECTION_BG : Theme::BG_ELEVATED);
        canvas.drawRoundRect(noX, Theme::CONTENT_Y + 70, nw, 18, 4,
                             !_selectedYes ? Theme::PRIMARY : Theme::BORDER);
        canvas.setCursor(noX + 10, Theme::CONTENT_Y + 72);
        canvas.print(no);
        Theme::useSmallFont(canvas);
    }

    // Version
    canvas.setTextColor(Theme::BORDER);
    const char* ver = "v" RATCOM_VERSION_STRING;
    int vw = strlen(ver) * Theme::CHAR_W;
    canvas.setCursor(cx - vw / 2, Theme::CONTENT_Y + Theme::CONTENT_H - 12);
    canvas.print(ver);
}

bool DataCleanScreen::handleKey(const KeyEvent& event) {
    if (_status) return true;  // Locked during wipe

    // ; = left/up, . = right/down (same as settings nav)
    if (event.character == ';') {
        _selectedYes = true;
        return true;
    }
    if (event.character == '.') {
        _selectedYes = false;
        return true;
    }
    if (event.enter) {
        if (_doneCb) _doneCb(_selectedYes);
        return true;
    }
    return true;  // Consume all keys
}

void DataCleanScreen::setStatus(const char* msg) {
    _status = msg;
}
