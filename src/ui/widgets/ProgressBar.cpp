#include "ProgressBar.h"

void ProgressBar::setProgress(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    _progress = progress;
}

void ProgressBar::render(M5Canvas& canvas, int x, int y, int w, int h) {
    canvas.fillRoundRect(x, y, w, h, 3, Theme::BG_ELEVATED);
    canvas.drawRoundRect(x, y, w, h, 3, Theme::BORDER);

    int fillW = (int)((w - 2) * _progress);
    if (fillW > 0) {
        canvas.fillRoundRect(x + 1, y + 1, fillW, h - 2, 2, Theme::PRIMARY);
    }
}
