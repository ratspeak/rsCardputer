#include "TextInput.h"

void TextInput::setText(const std::string& text) {
    _text = text;
    _cursorPos = _text.length();
}

void TextInput::clear() {
    _text.clear();
    _cursorPos = 0;
    _numericOnly = false;
}

bool TextInput::handleKey(const KeyEvent& event) {
    if (!_active) return false;

    // Enter → submit
    if (event.enter) {
        if (_submitCb && !_text.empty()) {
            _submitCb(_text);
        }
        return true;
    }

    // Backspace
    if (event.del) {
        if (_cursorPos > 0 && !_text.empty()) {
            _text.erase(_cursorPos - 1, 1);
            _cursorPos--;
        }
        return true;
    }

    // Regular character (includes space at ASCII 32)
    if (event.character >= 32 && event.character < 127) {
        if (_numericOnly && (event.character < '0' || event.character > '9')) {
            return true;  // Reject non-digit input
        }
        if ((int)_text.length() < _maxLength) {
            _text.insert(_text.begin() + _cursorPos, event.character);
            _cursorPos++;
        }
        return true;
    }

    // Space (fallback if character wasn't set but space flag is)
    if (event.space && event.character < 32) {
        if ((int)_text.length() < _maxLength) {
            _text.insert(_text.begin() + _cursorPos, ' ');
            _cursorPos++;
        }
        return true;
    }

    return false;
}

void TextInput::render(M5Canvas& canvas, int x, int y, int w) {
    int h = Theme::CHAR_H + 4;

    // Background and border
    canvas.fillRect(x, y, w, h, Theme::BG_ELEVATED);
    canvas.drawRect(x, y, w, h, _active ? Theme::PRIMARY : Theme::BORDER);

    // Prompt
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.setTextColor(Theme::ACCENT);
    canvas.setCursor(x + 2, y + 2);
    canvas.print("> ");

    // Text (scroll if too wide)
    int textAreaW = w - 4 - 2 * Theme::CHAR_W;  // Account for "> " prompt
    int maxChars = textAreaW / Theme::CHAR_W;
    int textX = x + 2 + 2 * Theme::CHAR_W;

    canvas.setTextColor(Theme::TEXT_PRIMARY);
    if ((int)_text.length() > maxChars) {
        int start = _cursorPos - maxChars + 1;
        if (start < 0) start = 0;
        canvas.setCursor(textX, y + 2);
        canvas.print(_text.substr(start, maxChars).c_str());
    } else {
        canvas.setCursor(textX, y + 2);
        canvas.print(_text.c_str());
    }

    // Cursor blink
    if (_active) {
        unsigned long now = millis();
        if (now - _lastBlink > 500) {
            _cursorVisible = !_cursorVisible;
            _lastBlink = now;
        }
        if (_cursorVisible) {
            int cursorX = textX + _cursorPos * Theme::CHAR_W;
            if ((int)_text.length() > maxChars) {
                int start = _cursorPos - maxChars + 1;
                if (start < 0) start = 0;
                cursorX = textX + (_cursorPos - start) * Theme::CHAR_W;
            }
            canvas.fillRect(cursorX, y + 1, Theme::CHAR_W, Theme::CHAR_H + 2, Theme::PRIMARY);
        }
    }
    Theme::useSmallFont(canvas);
}
