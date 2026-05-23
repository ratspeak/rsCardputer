#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>

#include "RsCardputerModeSwitch.h"

namespace {

constexpr uint16_t kBg = 0x0841;
constexpr uint16_t kPanel = 0x1082;
constexpr uint16_t kText = 0xF7BE;
constexpr uint16_t kMuted = 0x8C71;
constexpr uint16_t kAccent = 0x06D7;
constexpr uint16_t kWarn = 0xFBA0;
constexpr uint32_t kAutoBootMs = 7000;
constexpr char kPrefsNamespace[] = "rslaunch";
constexpr char kLastChoiceKey[] = "last";

enum class Choice : uint8_t {
  Ratcom = 0,
  RNode = 1,
};

Choice selected = Choice::Ratcom;
uint32_t bootStarted = 0;
uint32_t lastRemain = UINT32_MAX;
bool booting = false;
bool autoBootEnabled = true;

uint8_t choiceValue(Choice choice) {
  return choice == Choice::RNode ? 1 : 0;
}

Choice choiceFromValue(uint8_t value) {
  return value == 1 ? Choice::RNode : Choice::Ratcom;
}

Choice loadLastChoice() {
  Preferences prefs;
  Choice choice = Choice::Ratcom;
  if (prefs.begin(kPrefsNamespace, true)) {
    choice = choiceFromValue(prefs.getUChar(kLastChoiceKey, choiceValue(choice)));
    prefs.end();
  }
  return choice;
}

void saveLastChoice(Choice choice) {
  Preferences prefs;
  if (prefs.begin(kPrefsNamespace, false)) {
    prefs.putUChar(kLastChoiceKey, choiceValue(choice));
    prefs.end();
  }
}

void drawOption(int y, const char *title, const char *subtitle, bool active) {
  auto &d = M5Cardputer.Display;
  uint16_t fill = active ? kAccent : kPanel;
  uint16_t fg = active ? TFT_BLACK : kText;
  uint16_t sub = active ? 0x2104 : kMuted;

  d.fillRoundRect(12, y, 216, 34, 5, fill);
  d.setTextColor(fg, fill);
  d.setTextSize(1);
  d.setCursor(22, y + 7);
  d.print(title);
  d.setTextColor(sub, fill);
  d.setCursor(22, y + 20);
  d.print(subtitle);
}

uint32_t remainingSeconds() {
  uint32_t elapsed = millis() - bootStarted;
  if (elapsed >= kAutoBootMs) {
    return 0;
  }
  return (kAutoBootMs - elapsed + 999) / 1000;
}

void drawCountdown(bool force = false) {
  auto &d = M5Cardputer.Display;
  if (!autoBootEnabled) {
    d.fillRect(200, 8, 28, 20, kBg);
    lastRemain = UINT32_MAX;
    return;
  }

  uint32_t remain = remainingSeconds();
  if (!force && remain == lastRemain) {
    return;
  }
  lastRemain = remain;

  d.fillRect(200, 8, 28, 20, kBg);
  d.fillRoundRect(207, 9, 20, 18, 5, kPanel);
  d.setTextSize(1);
  d.setTextColor(kText, kPanel);
  d.setCursor(remain >= 10 ? 211 : 214, 15);
  d.print(static_cast<unsigned long>(remain));
}

void drawScreen() {
  auto &d = M5Cardputer.Display;
  d.fillScreen(kBg);

  d.setTextSize(2);
  d.setTextColor(kText, kBg);
  d.setCursor(12, 10);
  d.print("Ratspeak");

  d.setTextSize(1);
  d.setTextColor(kMuted, kBg);
  d.setCursor(14, 30);
  d.print("Cardputer Adv");

  drawOption(48, "Ratcom", "Standalone messenger", selected == Choice::Ratcom);
  drawOption(87, "RNode", "BLE / USB radio", selected == Choice::RNode);

  drawCountdown(true);
}

void selectChoice(Choice choice) {
  if (selected == choice) {
    return;
  }
  selected = choice;
  drawScreen();
}

void pauseAutoBoot() {
  if (!autoBootEnabled) {
    return;
  }
  autoBootEnabled = false;
  drawCountdown(true);
}

void showBooting(const char *label) {
  booting = true;
  auto &d = M5Cardputer.Display;
  d.fillScreen(kBg);
  d.setTextSize(2);
  d.setTextColor(kAccent, kBg);
  d.setCursor(20, 44);
  d.print(label);
  d.setTextSize(1);
  d.setTextColor(kMuted, kBg);
  d.setCursor(20, 75);
  d.print("Starting...");
}

void showError(const char *message) {
  booting = false;
  auto &d = M5Cardputer.Display;
  d.fillScreen(kBg);
  d.setTextSize(2);
  d.setTextColor(kWarn, kBg);
  d.setCursor(16, 36);
  d.print("Boot error");
  d.setTextSize(1);
  d.setTextColor(kText, kBg);
  d.setCursor(16, 68);
  d.print(message);
}

void startChoice(Choice choice) {
  using namespace rs_cardputer_adv;

  FirmwareMode mode = choice == Choice::Ratcom ? FirmwareMode::Ratcom : FirmwareMode::RNode;
  showBooting(mode_name(mode));
  SwitchResult result = set_next_boot(mode);
  if (!result.ok) {
    showError(result.message);
    return;
  }
  saveLastChoice(choice);
  delay(50);
  esp_restart();
}

void handleKey(const Keyboard_Class::KeysState &status) {
  if (status.enter) {
    startChoice(selected);
    return;
  }

  for (char key : status.word) {
    if (key == ';' || key == ',' || key == 'w' || key == 'W') {
      selectChoice(Choice::Ratcom);
      return;
    }
    if (key == '.' || key == '/' || key == 's' || key == 'S') {
      selectChoice(Choice::RNode);
      return;
    }
    if (key == 'r' || key == 'R') {
      startChoice(Choice::Ratcom);
      return;
    }
    if (key == 'n' || key == 'N') {
      startChoice(Choice::RNode);
      return;
    }
  }
}

} // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(180);

  selected = loadLastChoice();
  bootStarted = millis();
  drawScreen();
}

void loop() {
  if (booting) {
    delay(20);
    return;
  }

  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    pauseAutoBoot();
    handleKey(M5Cardputer.Keyboard.keysState());
  }

  drawCountdown();

  if (autoBootEnabled && millis() - bootStarted >= kAutoBootMs) {
    startChoice(selected);
    return;
  }
}
