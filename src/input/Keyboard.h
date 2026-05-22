#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>
#include <functional>

// Input modes
enum class InputMode {
    Navigation,  // Arrow-like movement, hotkeys active
    TextInput    // Character entry, Esc exits to Navigation
};

// Simplified key event for consumers
struct KeyEvent {
    char character;      // ASCII value (0 if special key)
    bool ctrl;
    bool shift;
    bool fn;
    bool alt;
    bool opt;
    bool enter;
    bool del;
    bool tab;
    bool space;
};

class Keyboard {
public:
    void begin();
    void update();

    // Mode control
    InputMode getMode() const { return _mode; }
    void setMode(InputMode mode) { _mode = mode; }

    // State queries
    bool hasEvent() const { return _hasEvent; }
    const KeyEvent& getEvent() const { return _event; }

    // For text input mode
    bool capsLocked() const;
    void setCapsLocked(bool locked);

    // Callback for key events (called after hotkey processing)
    using KeyCallback = std::function<void(const KeyEvent&)>;
    void setKeyCallback(KeyCallback cb) { _keyCallback = cb; }

private:
    bool pollHardware();

    InputMode _mode = InputMode::Navigation;
    KeyEvent _event = {};
    bool _hasEvent = false;
    unsigned long _lastHardwarePoll = 0;
    KeyCallback _keyCallback;
};
