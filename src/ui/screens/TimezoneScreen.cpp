#include "TimezoneScreen.h"
#include "ui/Theme.h"

void TimezoneScreen::onEnter() {
    _list.clear();
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
        _list.addItem(TIMEZONE_TABLE[i].label);
    }
    _list.setSelected(_selectedIdx);
}

void TimezoneScreen::render(M5Canvas& canvas) {
    int y = Theme::CONTENT_Y;

    const int headerH = 17;
    canvas.fillRect(0, y, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
    canvas.fillRect(0, y + 2, 3, headerH - 4, Theme::ACCENT);
    Theme::useUiFont(canvas);
    canvas.setTextColor(Theme::ACCENT);
    canvas.drawString("Select Timezone", 8, y + 2);
    canvas.drawFastHLine(0, y + headerH, Theme::CONTENT_W, Theme::DIVIDER);
    y += headerH + 2;

    Theme::useSmallFont(canvas);
    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("Choose your local region", 8, y);
    y += Theme::CHAR_H + 4;

    _list.render(canvas, 0, y, Theme::CONTENT_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
    Theme::useSmallFont(canvas);
}

bool TimezoneScreen::handleKey(const KeyEvent& event) {
    if (event.character == ';') {
        _list.scrollUp();
        return true;
    }
    if (event.character == '.') {
        _list.scrollDown();
        return true;
    }
    if (event.enter) {
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < TIMEZONE_COUNT && _doneCb) {
            _doneCb(idx);
        }
        return true;
    }
    return false;
}
