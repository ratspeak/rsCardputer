#include "ScrollList.h"

static const std::string EMPTY_ITEM;

namespace {
void drawFitted(M5Canvas& canvas, const std::string& text, int x, int y, int maxW) {
    if (canvas.textWidth(text.c_str()) <= maxW) {
        canvas.drawString(text.c_str(), x, y);
        return;
    }

    char buf[96];
    int copyLen = std::min((int)text.length(), (int)sizeof(buf) - 3);
    while (copyLen > 0) {
        memcpy(buf, text.c_str(), copyLen);
        buf[copyLen] = '.';
        buf[copyLen + 1] = '.';
        buf[copyLen + 2] = '\0';
        if (canvas.textWidth(buf) <= maxW) {
            canvas.drawString(buf, x, y);
            return;
        }
        copyLen--;
    }
}
}

void ScrollList::setItems(const std::vector<std::string>& items) {
    _items = items;
    _itemColors.assign(items.size(), 0);
    _selected = 0;
    _scrollOffset = 0;
}

void ScrollList::addItem(const std::string& item) {
    _items.push_back(item);
    _itemColors.push_back(0);
}

void ScrollList::addItem(const std::string& item, uint16_t color) {
    _items.push_back(item);
    _itemColors.push_back(color);
}

void ScrollList::clear() {
    _items.clear();
    _itemColors.clear();
    _selected = 0;
    _scrollOffset = 0;
}

void ScrollList::setSelected(int idx) {
    if (idx >= 0 && idx < (int)_items.size()) {
        _selected = idx;
        if (_visibleRows > 0) {
            if (_selected < _scrollOffset) _scrollOffset = _selected;
            else if (_selected >= _scrollOffset + _visibleRows)
                _scrollOffset = _selected - _visibleRows + 1;
        }
    }
}

void ScrollList::scrollUp() {
    if (_items.empty()) return;
    if (_selected > 0) {
        _selected--;
    } else {
        _selected = (int)_items.size() - 1;  // Wrap to bottom
    }
    if (_selected < _scrollOffset) {
        _scrollOffset = _selected;
    }
    if (_selected >= _scrollOffset + _visibleRows) {
        _scrollOffset = _selected - _visibleRows + 1;
    }
}

void ScrollList::scrollDown() {
    if (_items.empty()) return;
    if (_selected < (int)_items.size() - 1) {
        _selected++;
    } else {
        _selected = 0;  // Wrap to top
        _scrollOffset = 0;
    }
    if (_selected >= _scrollOffset + _visibleRows) {
        _scrollOffset = _selected - _visibleRows + 1;
    }
}

const std::string& ScrollList::getSelectedItem() const {
    if (_selected >= 0 && _selected < (int)_items.size()) {
        return _items[_selected];
    }
    return EMPTY_ITEM;
}

void ScrollList::render(M5Canvas& canvas, int x, int y, int w, int h) {
    int rowH = Theme::LIST_ROW_H;
    _visibleRows = h / rowH;
    if (_visibleRows < 1) _visibleRows = 1;

    Theme::useUiFont(canvas);

    for (int i = 0; i < _visibleRows && (i + _scrollOffset) < (int)_items.size(); i++) {
        int idx = i + _scrollOffset;
        int ry = y + i * rowH;
        bool selected = (idx == _selected);

        if (selected) {
            canvas.fillRoundRect(x + 1, ry, w - 2, rowH - 1, 3, Theme::BG_HOVER);
            canvas.fillRoundRect(x + 2, ry + 2, 3, rowH - 5, 1, Theme::PRIMARY);
            canvas.setTextColor(Theme::TEXT_PRIMARY);
        } else {
            uint16_t itemColor = (idx < (int)_itemColors.size() && _itemColors[idx] != 0)
                                 ? _itemColors[idx] : Theme::TEXT_SECONDARY;
            canvas.setTextColor(itemColor);
        }

        if (!selected && (i + 1 < _visibleRows) && (idx + 1 < (int)_items.size())) {
            canvas.drawFastHLine(x + 8, ry + rowH - 1, w - 12, Theme::DIVIDER);
        }

        int textX = x + 9;
        int textW = w - 16;
        drawFitted(canvas, _items[idx], textX, ry + 1, textW);
    }

    if ((int)_items.size() > _visibleRows) {
        int barH = h * _visibleRows / _items.size();
        if (barH < 6) barH = 6;
        int barY = y + (_scrollOffset * h / _items.size());
        canvas.fillRoundRect(x + w - 3, y, 2, h, 1, Theme::DIVIDER);
        canvas.fillRoundRect(x + w - 4, barY, 3, barH, 1, Theme::PRIMARY_MUTED);
    }
    Theme::useSmallFont(canvas);
}
