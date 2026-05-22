#include "NameInputScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"

void NameInputScreen::render(M5Canvas& canvas) {
    int cx = Theme::SCREEN_W / 2;

    // "RATSPEAK" title
    canvas.setTextSize(2);
    canvas.setTextColor(Theme::PRIMARY);
    const char* title = "RATSPEAK";
    int tw = strlen(title) * 12;
    canvas.setCursor(cx - tw / 2, Theme::CONTENT_Y + 8);
    canvas.print(title);

    // "ratspeak.org" subtitle
    canvas.setTextSize(1);
    canvas.setTextColor(Theme::MUTED);
    const char* sub = "ratspeak.org";
    int sw = strlen(sub) * Theme::CHAR_W;
    canvas.setCursor(cx - sw / 2, Theme::CONTENT_Y + 28);
    canvas.print(sub);

    // Prompt
    canvas.setTextColor(Theme::SECONDARY);
    const char* prompt = "Enter your display name";
    int pw = strlen(prompt) * Theme::CHAR_W;
    canvas.setCursor(cx - pw / 2, Theme::CONTENT_Y + 46);
    canvas.print(prompt);

    // Input field
    int fieldW = MAX_NAME_LEN * Theme::CHAR_W + 8;
    int fieldX = cx - fieldW / 2;
    int fieldY = Theme::CONTENT_Y + 60;
    int fieldH = Theme::CHAR_H + 6;

    canvas.drawRect(fieldX, fieldY, fieldW, fieldH, Theme::PRIMARY);
    canvas.setTextColor(Theme::PRIMARY);
    canvas.setCursor(fieldX + 4, fieldY + 3);
    canvas.print(_name);

    // Cursor blink
    if ((millis() / 500) % 2 == 0) {
        int curX = fieldX + 4 + _nameLen * Theme::CHAR_W;
        canvas.drawFastVLine(curX, fieldY + 2, Theme::CHAR_H, Theme::PRIMARY);
    }

    // Version
    canvas.setTextColor(Theme::BORDER);
    const char* ver = "v" RATCOM_VERSION_STRING;
    int vw = strlen(ver) * Theme::CHAR_W;
    canvas.setCursor(cx - vw / 2, Theme::CONTENT_Y + Theme::CONTENT_H - 12);
    canvas.print(ver);
}

bool NameInputScreen::handleKey(const KeyEvent& event) {
    if (event.enter) {
        if (_doneCb) _doneCb(String(_name));
        return true;
    }
    if (event.del && _nameLen > 0) {
        _nameLen--;
        _name[_nameLen] = 0;
        return true;
    }
    char c = event.character;
    if (c >= 32 && c < 127 && _nameLen < MAX_NAME_LEN) {
        _name[_nameLen++] = c;
        _name[_nameLen] = 0;
        return true;
    }
    return false;
}
