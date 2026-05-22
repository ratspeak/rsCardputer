#include "TabBar.h"
#include "Theme.h"

constexpr const char* TabBar::TAB_LABELS[4];

void TabBar::setActiveTab(int tab) {
    if (tab >= 0 && tab < Theme::TAB_COUNT) {
        _activeTab = tab;
        if (_dirty) *_dirty = true;
    }
}

void TabBar::cycleTab(int direction) {
    _activeTab = (_activeTab + direction + Theme::TAB_COUNT) % Theme::TAB_COUNT;
    if (_dirty) *_dirty = true;
}

void TabBar::setUnreadCount(int tab, int count) {
    if (tab >= 0 && tab < Theme::TAB_COUNT) {
        _unreadCounts[tab] = count;
        if (_dirty) *_dirty = true;
    }
}

void TabBar::render(M5Canvas& canvas) {
    int y = Theme::SCREEN_H - Theme::TAB_BAR_H;

    canvas.fillRect(0, y, Theme::SCREEN_W, Theme::TAB_BAR_H, Theme::BAR_BG);
    canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::DIVIDER);

    Theme::useUiFont(canvas);

    for (int i = 0; i < Theme::TAB_COUNT; i++) {
        int tx = i * Theme::TAB_W;
        bool active = (i == _activeTab);

        int labelLen = canvas.textWidth(TAB_LABELS[i]);
        int labelX = tx + (Theme::TAB_W - labelLen) / 2;
        int labelY = y + 3;

        if (active) {
            int pillW = labelLen + 10;
            int pillX = tx + (Theme::TAB_W - pillW) / 2;
            int pillY = y + 2;
            int pillH = Theme::TAB_BAR_H - 4;
            canvas.fillRoundRect(pillX, pillY, pillW, pillH, 3, Theme::SELECTION_BG);
            canvas.drawRoundRect(pillX, pillY, pillW, pillH, 3, Theme::PRIMARY);
        }

        // Label color — blink Msgs tab when unread
        uint16_t labelColor;
        if (active) {
            labelColor = Theme::TAB_ACTIVE;
        } else if (i == TAB_MSGS && _unreadCounts[TAB_MSGS] > 0) {
            bool blinkOn = ((millis() / 1500) % 2) == 0;
            labelColor = blinkOn ? Theme::WARNING : Theme::TAB_INACTIVE;
        } else {
            labelColor = Theme::TAB_INACTIVE;
        }
        canvas.setTextColor(labelColor);
        canvas.setCursor(labelX, labelY);
        canvas.print(TAB_LABELS[i]);

        if (_unreadCounts[i] > 0) {
            char badge[8];
            snprintf(badge, sizeof(badge), "%d", _unreadCounts[i]);
            Theme::useSmallFont(canvas);
            int badgeW = strlen(badge) * Theme::CHAR_W + 6;
            int badgeX = tx + Theme::TAB_W - badgeW - 3;
            int badgeY = y + 3;
            canvas.fillRoundRect(badgeX, badgeY, badgeW, Theme::CHAR_H + 3, 3, Theme::BADGE_BG);
            canvas.setTextColor(Theme::BADGE_TEXT);
            canvas.setCursor(badgeX + 3, badgeY + 1);
            canvas.print(badge);
            Theme::useUiFont(canvas);
        }
    }
    Theme::useSmallFont(canvas);
}
