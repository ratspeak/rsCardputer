#include "UIManager.h"

void UIManager::begin() {
    // 8-bit palette mode: 240×135×1 = 32,400 bytes (vs 64,800 at 16-bit)
    // Saves 32KB heap — critical on this no-PSRAM device
    _canvas.setColorDepth(8);
    _canvas.createSprite(Theme::SCREEN_W, Theme::SCREEN_H);
    // Register theme colors in palette — M5GFX auto-matches when drawing
    _canvas.setPaletteColor(0, Theme::BG);
    _canvas.setPaletteColor(1, Theme::PRIMARY);
    _canvas.setPaletteColor(2, Theme::SECONDARY);
    _canvas.setPaletteColor(3, Theme::MUTED);
    _canvas.setPaletteColor(4, Theme::ERROR);
    _canvas.setPaletteColor(5, Theme::WARNING);
    _canvas.setPaletteColor(6, Theme::ACCENT);
    _canvas.setPaletteColor(7, Theme::BORDER);
    _canvas.setPaletteColor(8, Theme::SELECTION_BG);
    _canvas.setPaletteColor(9, Theme::BAR_BG);
    _canvas.setPaletteColor(10, Theme::BADGE_BG);
    _canvas.setPaletteColor(11, Theme::BADGE_TEXT);
    _canvas.setPaletteColor(12, Theme::BG_ELEVATED);
    _canvas.setPaletteColor(13, Theme::BG_SURFACE);
    _canvas.setPaletteColor(14, Theme::BG_HOVER);
    _canvas.setPaletteColor(15, Theme::TEXT_PRIMARY);
    _canvas.setPaletteColor(16, Theme::TEXT_SECONDARY);
    _canvas.setPaletteColor(17, Theme::TEXT_MUTED);
    _canvas.setPaletteColor(18, Theme::PRIMARY_MUTED);
    _canvas.setPaletteColor(19, Theme::PRIMARY_SUBTLE);
    _canvas.setPaletteColor(20, Theme::SUCCESS);
    _canvas.setPaletteColor(21, Theme::DIVIDER);
    Serial.printf("[UI] Canvas: 8-bit palette, %d bytes\n", Theme::SCREEN_W * Theme::SCREEN_H);
    Theme::useSmallFont(_canvas);
    _canvas.fillScreen(Theme::BG);
    _needsRender = true;
    _statusDirty = true;
    _contentDirty = true;
    _tabDirty = true;

    // Wire up dirty flag callbacks
    _statusBar.setDirtyFlag(&_statusDirty);
    _tabBar.setDirtyFlag(&_tabDirty);
}

void UIManager::setScreen(Screen* screen) {
    if (_currentScreen) {
        _currentScreen->onExit();
    }
    _currentScreen = screen;
    if (_currentScreen) {
        _currentScreen->onEnter();
    }
    markAllDirty();
}

void UIManager::render() {
    if (_bootMode) {
        // Boot mode: always render full screen
        _canvas.fillScreen(Theme::BG);
        Theme::useSmallFont(_canvas);
        if (_currentScreen) {
            _currentScreen->render(_canvas);
        }
        flush();
        return;
    }

    // Skip render if nothing changed
    if (!_statusDirty && !_contentDirty && !_tabDirty) return;

    // Full canvas redraw (M5Canvas doesn't support partial push)
    _canvas.fillScreen(Theme::BG);
    Theme::useSmallFont(_canvas);
    _statusBar.render(_canvas);

    if (_currentScreen) {
        _canvas.setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
        Theme::useSmallFont(_canvas);
        _currentScreen->render(_canvas);
        _canvas.clearClipRect();
    }

    Theme::useSmallFont(_canvas);
    _tabBar.render(_canvas);

    if (_overlay) {
        _canvas.setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
        Theme::useSmallFont(_canvas);
        _overlay->render(_canvas);
        _canvas.clearClipRect();
    }

    flush();
    _statusDirty = _contentDirty = _tabDirty = false;
}

void UIManager::flush() {
    _canvas.pushSprite(&M5.Display, 0, 0);
}

bool UIManager::handleKey(const KeyEvent& event) {
    markContentDirty();
    if (_currentScreen) {
        return _currentScreen->handleKey(event);
    }
    return false;
}
