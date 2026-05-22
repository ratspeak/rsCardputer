#include "SettingsScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"
#include <algorithm>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>

void SettingsScreen::onEnter() {
    _subMenu = MENU_MAIN;
    _editing = false;
    _editField = -1;
    buildMainMenu();
}

void SettingsScreen::buildMainMenu() {
    _list.clear();
    _list.addItem("Radio");
    _list.addItem("Wi-Fi");
    _list.addItem("SD Card");
    _list.addItem("Display");
    _list.addItem("Audio");
    _list.addItem("About");
    _list.addItem("Factory Reset", Theme::ERROR);
}

struct RadioPreset {
    const char* name;
    uint8_t sf; uint32_t bw; uint8_t cr; int8_t txPower;
};
static const RadioPreset PRESETS[] = {
    {"Short Turbo",   7,  500000, 5,  14},
    {"Short Fast",    7,  250000, 5,  14},
    {"Short Slow",    8,  250000, 5,  14},
    {"Medium Fast",   9,  250000, 5,  17},
    {"Medium Slow",   10, 250000, 5,  17},
    {"Long Turbo",    11, 500000, 8,  LORA_MAX_TX_POWER},
    {"Long Fast",     11, 250000, 5,  LORA_MAX_TX_POWER},
    {"Long Moderate", 11, 125000, 8,  LORA_MAX_TX_POWER},
};
static constexpr int NUM_PRESETS = 8;

static int detectPresetIndex(const UserSettings& s) {
    for (int i = 0; i < NUM_PRESETS; i++) {
        if (s.loraSF == PRESETS[i].sf && s.loraBW == PRESETS[i].bw
            && s.loraCR == PRESETS[i].cr && s.loraTxPower == PRESETS[i].txPower)
            return i;
    }
    return -1;
}

static const char* detectPresetName(const UserSettings& s) {
    int idx = detectPresetIndex(s);
    return idx >= 0 ? PRESETS[idx].name : "Custom";
}

void SettingsScreen::buildRadioMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    // Active preset indicator
    snprintf(buf, sizeof(buf), "Active: %s", detectPresetName(s));
    _list.addItem(buf);

    // Presets (items 1..NUM_PRESETS)
    int activeIdx = detectPresetIndex(s);
    for (int i = 0; i < NUM_PRESETS; i++) {
        char label[40];
        snprintf(label, sizeof(label), "%s[%s]",
                 (i == activeIdx) ? ">" : " ", PRESETS[i].name);
        _list.addItem(label, (i == activeIdx) ? Theme::PRIMARY : 0);
    }

    // Editable fields (items NUM_PRESETS+1 .. NUM_PRESETS+5)
    snprintf(buf, sizeof(buf), "Frequency: %lu Hz", (unsigned long)s.loraFrequency);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "SF: %d", s.loraSF);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "BW: %lu Hz", (unsigned long)s.loraBW);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "CR: %d", s.loraCR);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "TX Power: %d dBm", s.loraTxPower);
    _list.addItem(buf);

    _list.addItem("< Back");
}

void SettingsScreen::buildWiFiMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();

    // Item 0: Mode selector
    const char* modeNames[] = {"OFF", "AP", "STA"};
    char modeBuf[24];
    snprintf(modeBuf, sizeof(modeBuf), "Mode: %s", modeNames[s.wifiMode]);
    _list.addItem(modeBuf);

    if (s.wifiMode == RAT_WIFI_AP) {
        // Items 1-2: AP fields
        String apSSID = s.wifiAPSSID.isEmpty() ? "(auto)" : s.wifiAPSSID;
        _list.addItem(("AP SSID: " + std::string(apSSID.c_str())));
        _list.addItem(("AP Pass: " + std::string(s.wifiAPPassword.c_str())));
    } else if (s.wifiMode == RAT_WIFI_STA) {
        // Item 1: Connection status
        if (WiFi.status() == WL_CONNECTED) {
            char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "Connected: %s", WiFi.SSID().c_str());
            _list.addItem(statusBuf);
            _list.addItem("[Disconnect]");          // Item 2
        } else if (!s.wifiSTASSID.isEmpty()) {
            char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "Saved: %s (offline)", s.wifiSTASSID.c_str());
            _list.addItem(statusBuf);
            _list.addItem("[Connect]");             // Item 2
        } else {
            _list.addItem("No network configured");
            _list.addItem("");                      // Item 2 placeholder
        }
        _list.addItem("Scan Networks");             // Item 3
        _list.addItem("TCP Connections");            // Item 4
        // Item 5: AutoInterface (LAN auto-discovery via IPv6 multicast)
        char autoBuf[40];
        snprintf(autoBuf, sizeof(autoBuf), "Auto-discover LAN: %s",
                 s.autoIfaceEnabled ? "ON" : "OFF");
        _list.addItem(autoBuf);
    }
    // OFF mode: only mode selector + back

    _list.addItem("< Back");
}

void SettingsScreen::buildTCPMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();

    _list.addItem("+ Add Connection");

    for (size_t i = 0; i < s.tcpConnections.size(); i++) {
        auto& ep = s.tcpConnections[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:%d [%s]",
                 ep.host.c_str(), ep.port,
                 ep.autoConnect ? "ON" : "OFF");
        _list.addItem(buf);
    }

    _list.addItem("< Back");
}

void SettingsScreen::addTCPConnection(const std::string& host, uint16_t port) {
    if (!_config) return;
    auto& s = _config->settings();
    if (s.tcpConnections.size() >= MAX_TCP_CONNECTIONS) {
        showToast("Max 4 connections");
        return;
    }

    TCPEndpoint ep;
    ep.host = host.c_str();
    ep.port = port;
    if (ep.host.isEmpty()) return;

    // Test connection before adding (only if WiFi is connected)
    if (WiFi.status() == WL_CONNECTED) {
        showToast("Testing connection...");
        WiFiClient testClient;
        if (!testClient.connect(ep.host.c_str(), ep.port, 3000)) {
            testClient.stop();
            showToast("Connection failed!", 2500);
            Serial.printf("[TCP] Test failed: %s:%d\n", ep.host.c_str(), ep.port);
            buildTCPMenu();
            return;
        }
        testClient.stop();
    }

    s.tcpConnections.push_back(ep);
    applyAndSave();
    showToast("Added! Reboot to connect");
    buildTCPMenu();
}

void SettingsScreen::toggleTCPConnection(int index) {
    if (!_config) return;
    auto& s = _config->settings();
    if (index < 0 || index >= (int)s.tcpConnections.size()) return;

    s.tcpConnections[index].autoConnect = !s.tcpConnections[index].autoConnect;
    applyAndSave();
    buildTCPMenu();
}

void SettingsScreen::removeTCPConnection(int index) {
    if (!_config) return;
    auto& s = _config->settings();
    if (index < 0 || index >= (int)s.tcpConnections.size()) return;

    s.tcpConnections.erase(s.tcpConnections.begin() + index);
    applyAndSave();
    showToast("Removed");
    buildTCPMenu();
}

// =============================================================================
// SD Card submenu
// =============================================================================

void SettingsScreen::buildSDCardMenu() {
    _list.clear();

    if (_sdStore && _sdStore->isReady()) {
        _list.addItem("Status: INSERTED");

        char buf[32];
        uint64_t total = _sdStore->totalBytes();
        uint64_t used = _sdStore->usedBytes();
        uint64_t free = total > used ? total - used : 0;

        snprintf(buf, sizeof(buf), "Total: %llu MB", total / (1024 * 1024));
        _list.addItem(buf);
        snprintf(buf, sizeof(buf), "Used: %llu MB", used / (1024 * 1024));
        _list.addItem(buf);
        snprintf(buf, sizeof(buf), "Free: %llu MB", free / (1024 * 1024));
        _list.addItem(buf);

        _list.addItem("Initialize for RatCom");
        _list.addItem("Wipe All Data", Theme::ERROR);
    } else {
        _list.addItem("Status: NOT INSERTED");
    }

    _list.addItem("< Back");
}

void SettingsScreen::sdCardFormat() {
    if (!_sdStore || !_sdStore->isReady()) {
        showToast("No SD card");
        return;
    }

    if (_sdStore->formatForRatcom()) {
        showToast("SD initialized!");
    } else {
        showToast("Init failed");
    }
    buildSDCardMenu();
}

// =============================================================================
// WiFi Scanner
// =============================================================================

void SettingsScreen::startWiFiScan() {
    _list.clear();
    _list.addItem("Scanning...");

    Serial.println("[WIFI] Starting network scan...");

    // Disconnect from current network to free the radio for scanning
    WiFi.disconnect(false);  // disconnect but don't erase credentials
    delay(100);

    // Ensure WiFi is on in STA mode for scanning
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }

    int n = WiFi.scanNetworks(false, false);
    _scanResults.clear();

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            WiFiNetwork net;
            net.ssid = WiFi.SSID(i);
            net.rssi = WiFi.RSSI(i);
            net.encType = WiFi.encryptionType(i);
            if (!net.ssid.isEmpty()) {
                _scanResults.push_back(net);
            }
        }
        // Sort by signal strength (strongest first)
        std::sort(_scanResults.begin(), _scanResults.end(),
            [](const WiFiNetwork& a, const WiFiNetwork& b) {
                return a.rssi > b.rssi;
            });
    }

    WiFi.scanDelete();
    Serial.printf("[WIFI] Found %d networks\n", (int)_scanResults.size());

    _subMenu = MENU_WIFI_SCAN;
    buildScanResultsMenu();
}

void SettingsScreen::buildScanResultsMenu() {
    _list.clear();

    if (_scanResults.empty()) {
        _list.addItem("No networks found");
    } else {
        for (auto& net : _scanResults) {
            char buf[48];
            const char* lock = (net.encType == WIFI_AUTH_OPEN) ? "" : "*";
            snprintf(buf, sizeof(buf), "%s%s (%d dBm)",
                     lock, net.ssid.c_str(), net.rssi);
            _list.addItem(buf);
        }
    }

    _list.addItem("Rescan");
    _list.addItem("< Back");
}

void SettingsScreen::disconnectWiFi() {
    WiFi.disconnect(false);
    showToast("Disconnected");
    buildWiFiMenu();
}

void SettingsScreen::connectWiFi() {
    auto& s = _config->settings();
    if (s.wifiSTASSID.isEmpty()) {
        showToast("No SSID set");
        return;
    }
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin(s.wifiSTASSID.c_str(), s.wifiSTAPassword.c_str());
    showToast("Connecting...");
    buildWiFiMenu();
}

void SettingsScreen::selectNetwork(int index) {
    if (index < 0 || index >= (int)_scanResults.size()) return;
    if (!_config) return;

    auto& s = _config->settings();
    s.wifiSTASSID = _scanResults[index].ssid;
    Serial.printf("[WIFI] Selected: %s\n", s.wifiSTASSID.c_str());

    // Open password editor
    _subMenu = MENU_WIFI;
    // field 1 = STA password in WiFi STA mode
    startEditing(1, s.wifiSTAPassword.c_str());
}

// =============================================================================
// Display / Audio menus
// =============================================================================

void SettingsScreen::buildDisplayMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    snprintf(buf, sizeof(buf), "Brightness: %d", s.brightness);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "Dim timeout: %ds", s.screenDimTimeout);
    _list.addItem(buf);
    snprintf(buf, sizeof(buf), "Off timeout: %ds", s.screenOffTimeout);
    _list.addItem(buf);

    String name = s.displayName.isEmpty() ? "(none)" : s.displayName;
    _list.addItem(("Name: " + std::string(name.c_str())));
    _list.addItem("< Back");
}

void SettingsScreen::buildAudioMenu() {
    _list.clear();
    if (!_config) return;
    auto& s = _config->settings();
    char buf[40];

    _list.addItem(s.audioEnabled ? "Audio: ON" : "Audio: OFF");
    snprintf(buf, sizeof(buf), "Volume: %d%%", s.audioVolume);
    _list.addItem(buf);
    _list.addItem("< Back");
}

// Start editing a field — show TextInput with current value
void SettingsScreen::startEditing(int field, const std::string& currentValue) {
    _editField = field;
    _editing = true;
    _editInput.clear();
    _editInput.setText(currentValue);
    _editInput.setActive(true);
    _editInput.setMaxLength(64);
    _editInput.setSubmitCallback([this](const std::string& value) {
        commitEdit(value);
    });
}

// Apply edited value to settings
void SettingsScreen::commitEdit(const std::string& value) {
    if (!_config) return;
    auto& s = _config->settings();

    if (_subMenu == MENU_RADIO) {
        long v = atol(value.c_str());
        switch (_editField) {
            case 0:  // Frequency: 150MHz-960MHz
                if (v >= 150000000L && v <= 960000000L) s.loraFrequency = (uint32_t)v;
                break;
            case 1:  // SF: 5-12
                if (v >= 5 && v <= 12) s.loraSF = (uint8_t)v;
                break;
            case 2:  // BW: 7800-500000
                if (v >= 7800 && v <= 500000) s.loraBW = (uint32_t)v;
                break;
            case 3:  // CR: 5-8
                if (v >= 5 && v <= 8) s.loraCR = (uint8_t)v;
                break;
            case 4:  // TX Power
                if (v >= -9 && v <= LORA_MAX_TX_POWER) s.loraTxPower = (int8_t)v;
                break;
        }
        applyAndSave();
        buildRadioMenu();
    } else if (_subMenu == MENU_TCP) {
        if (_editField == 99 && !value.empty()) {
            // Host submitted — stash and open port input
            _tcpPendingHost = value;
            _editField = 100;
            _editing = true;
            _editLabel = "Port (1-9999):";
            _editInput.clear();
            _editInput.setText("4242");
            _editInput.setActive(true);
            _editInput.setMaxLength(5);
            _editInput.setNumericOnly(true);
            _editInput.setSubmitCallback([this](const std::string& v) {
                commitEdit(v);
            });
            return;  // Stay in editing mode
        }
        if (_editField == 100 && !value.empty()) {
            // Port submitted — validate and add
            int port = atoi(value.c_str());
            if (port < 1 || port > 9999) {
                showToast("Port 1-9999");
                buildTCPMenu();
            } else {
                addTCPConnection(_tcpPendingHost, (uint16_t)port);
            }
            _tcpPendingHost.clear();
        }
        buildTCPMenu();
    } else if (_subMenu == MENU_WIFI) {
        // Fields: 0=SSID, 1=Password (AP or STA depending on mode)
        if (_config->settings().wifiMode == RAT_WIFI_AP) {
            switch (_editField) {
                case 0: s.wifiAPSSID = value.c_str(); break;
                case 1: s.wifiAPPassword = value.c_str(); break;
            }
        } else if (_config->settings().wifiMode == RAT_WIFI_STA) {
            switch (_editField) {
                case 0: s.wifiSTASSID = value.c_str(); break;
                case 1: s.wifiSTAPassword = value.c_str(); break;
            }
        }
        applyAndSave();
        if (_config->settings().wifiMode == RAT_WIFI_STA && _editField == 1) {
            connectWiFi();  // Live reconnect with new credentials
        } else {
            showToast("Saved!");
        }
        buildWiFiMenu();
    } else if (_subMenu == MENU_DISPLAY) {
        switch (_editField) {
            case 0: {
                int v = atoi(value.c_str());
                if (v < 1) v = 1;
                if (v > 255) v = 255;
                s.brightness = (uint8_t)v;
                if (_power) _power->setBrightness(s.brightness);
                break;
            }
            case 1: s.screenDimTimeout = (uint16_t)atoi(value.c_str()); break;
            case 2: s.screenOffTimeout = (uint16_t)atoi(value.c_str()); break;
            case 3: s.displayName = value.c_str(); break;
        }
        applyAndSave();
        buildDisplayMenu();
    } else if (_subMenu == MENU_AUDIO) {
        if (_editField == 1) {
            int v = atoi(value.c_str());
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            s.audioVolume = (uint8_t)v;
            if (_audio) _audio->setVolume(s.audioVolume);
        }
        applyAndSave();
        buildAudioMenu();
    }

    _editing = false;
    _editField = -1;
}

// Get current value of a field as string for editing
std::string SettingsScreen::getCurrentValue(SubMenu menu, int field) {
    if (!_config) return "";
    auto& s = _config->settings();
    char buf[32];

    if (menu == MENU_RADIO) {
        switch (field) {
            case 0: snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.loraFrequency); return buf;
            case 1: snprintf(buf, sizeof(buf), "%d", s.loraSF); return buf;
            case 2: snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.loraBW); return buf;
            case 3: snprintf(buf, sizeof(buf), "%d", s.loraCR); return buf;
            case 4: snprintf(buf, sizeof(buf), "%d", s.loraTxPower); return buf;
        }
    } else if (menu == MENU_WIFI) {
        if (s.wifiMode == RAT_WIFI_AP) {
            switch (field) {
                case 0: return s.wifiAPSSID.c_str();
                case 1: return s.wifiAPPassword.c_str();
            }
        } else if (s.wifiMode == RAT_WIFI_STA) {
            switch (field) {
                case 0: return s.wifiSTASSID.c_str();
                case 1: return s.wifiSTAPassword.c_str();
            }
        }
    } else if (menu == MENU_DISPLAY) {
        switch (field) {
            case 0: snprintf(buf, sizeof(buf), "%d", s.brightness); return buf;
            case 1: snprintf(buf, sizeof(buf), "%d", s.screenDimTimeout); return buf;
            case 2: snprintf(buf, sizeof(buf), "%d", s.screenOffTimeout); return buf;
            case 3: return s.displayName.c_str();
        }
    } else if (menu == MENU_AUDIO) {
        if (field == 1) { snprintf(buf, sizeof(buf), "%d", s.audioVolume); return buf; }
    }
    return "";
}

void SettingsScreen::render(M5Canvas& canvas) {
    if (_subMenu == MENU_ABOUT) {
        renderAbout(canvas);
        return;
    }

    int y0 = Theme::CONTENT_Y;

    // Header with accent bar
    const char* headers[] = {"SETTINGS", "RADIO", "WIFI", "TCP CONNECTIONS",
                             "SD CARD", "DISPLAY", "AUDIO", "ABOUT", "WIFI SCAN"};
    const int headerH = 17;
    canvas.fillRect(0, y0, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
    canvas.fillRect(0, y0 + 2, 3, headerH - 4, Theme::ACCENT);
    canvas.setTextColor(Theme::ACCENT);
    Theme::useUiFont(canvas);
    canvas.drawString(headers[_subMenu], 8, y0 + 2);
    canvas.drawFastHLine(0, y0 + headerH, Theme::CONTENT_W, Theme::DIVIDER);
    Theme::useSmallFont(canvas);

    if (_editing) {
        // Show field name
        canvas.setTextColor(Theme::MUTED);
        std::string label = _editLabel.empty() ? "Edit value:" : _editLabel;
        canvas.drawString(label.c_str(), 4, y0 + headerH + 5);

        // Show text input
        _editInput.render(canvas, 0, y0 + headerH + 19, Theme::CONTENT_W);

        // Hint
        canvas.setTextColor(Theme::MUTED);
        canvas.drawString("Enter=save  Esc=cancel", 4, y0 + headerH + 36);
    } else {
        _list.render(canvas, 0, y0 + headerH + 2, Theme::CONTENT_W,
                     Theme::CONTENT_H - headerH - 3);
    }

    // Confirmation dialog overlay
    if (_confirmPending) {
        const char* prompt = _confirmAction == 0 ?
            "Factory Reset? Y/N" : "Wipe SD Data? Y/N";
        int tw = strlen(prompt) * Theme::CHAR_W + 16;
        int th = Theme::CHAR_H + 10;
        int tx = (Theme::CONTENT_W - tw) / 2;
        int ty = Theme::CONTENT_Y + Theme::CONTENT_H / 2 - th / 2;
        canvas.fillRoundRect(tx, ty, tw, th, 3, Theme::BG);
        canvas.drawRoundRect(tx, ty, tw, th, 3, Theme::ERROR);
        canvas.setTextColor(Theme::ERROR);
        canvas.setCursor(tx + 8, ty + 5);
        canvas.print(prompt);
    }

    // Toast overlay (drawn on top of everything)
    if (_toastMessage && millis() < _toastUntil) {
        int tw = strlen(_toastMessage) * Theme::CHAR_W + 12;
        int th = Theme::CHAR_H + 8;
        int tx = (Theme::CONTENT_W - tw) / 2;
        int ty = Theme::CONTENT_Y + Theme::CONTENT_H - th - 4;
        canvas.fillRoundRect(tx, ty, tw, th, 3, Theme::SELECTION_BG);
        canvas.drawRoundRect(tx, ty, tw, th, 3, Theme::PRIMARY);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(tx + 6, ty + 4);
        canvas.print(_toastMessage);
    } else {
        _toastMessage = nullptr;
    }
}

void SettingsScreen::renderAbout(M5Canvas& canvas) {
    int y0 = Theme::CONTENT_Y;
    const int headerH = 17;
    canvas.fillRect(0, y0, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
    canvas.fillRect(0, y0 + 2, 3, headerH - 4, Theme::ACCENT);
    Theme::useUiFont(canvas);
    canvas.setTextColor(Theme::ACCENT);
    canvas.drawString("About", 8, y0 + 2);
    canvas.drawFastHLine(0, y0 + headerH, Theme::CONTENT_W, Theme::DIVIDER);

    Theme::useSmallFont(canvas);
    int y = y0 + headerH + 5;
    canvas.setTextColor(Theme::PRIMARY);
    canvas.drawString("RatCom v" RATCOM_VERSION_STRING, 4, y); y += 10;

    canvas.setTextColor(Theme::SECONDARY);
    canvas.drawString("M5Stack Cardputer Adv", 4, y); y += 10;
    canvas.drawString("Cap LoRa-1262 (SX1262)", 4, y); y += 10;

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("ratspeak.org", 4, y); y += 12;

    canvas.setTextColor(Theme::SECONDARY);
    String idLine = "ID: " + _identityHash;
    canvas.drawString(idLine.c_str(), 4, y); y += 10;

    char heap[32];
    snprintf(heap, sizeof(heap), "Heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
    canvas.drawString(heap, 4, y); y += 10;

    char uptime[32];
    snprintf(uptime, sizeof(uptime), "Up: %lus", millis() / 1000);
    canvas.drawString(uptime, 4, y); y += 12;

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("[Esc: back]", 4, y);
}

bool SettingsScreen::handleKey(const KeyEvent& event) {
    // Handle confirmation dialog
    if (_confirmPending) {
        if (event.character == 'y' || event.character == 'Y') {
            _confirmPending = false;
            if (_confirmAction == 0) {
                factoryReset();
            } else {
                if (_sdStore && _sdStore->isReady()) {
                    if (_sdStore->wipeRatcom()) {
                        showToast("SD wiped!");
                    } else {
                        showToast("Wipe failed");
                    }
                    buildSDCardMenu();
                }
            }
        } else {
            _confirmPending = false;
            showToast("Cancelled");
        }
        return true;
    }

    // ESC or Delete goes back
    if (event.character == 27 || (event.del && !_editing)) {
        if (_editing) {
            _editing = false;
            _editField = -1;
            _tcpPendingHost.clear();
            _editLabel.clear();
            return true;
        }
        if (_subMenu == MENU_TCP) {
            _subMenu = MENU_WIFI;
            buildWiFiMenu();
            return true;
        }
        if (_subMenu == MENU_WIFI_SCAN) {
            _subMenu = MENU_WIFI;
            buildWiFiMenu();
            return true;
        }
        if (_subMenu != MENU_MAIN) {
            _subMenu = MENU_MAIN;
            buildMainMenu();
            return true;
        }
        return false;
    }

    if (_editing) {
        if (_editInput.handleKey(event)) return true;
        return false;
    }

    // Delete key in TCP submenu removes selected connection
    if (event.del && _subMenu == MENU_TCP) {
        int sel = _list.getSelectedIndex();
        int tcpIdx = sel - 1;  // item 0 is "Add", items 1..N are connections
        if (tcpIdx >= 0 && tcpIdx < (int)_config->settings().tcpConnections.size()) {
            removeTCPConnection(tcpIdx);
            return true;
        }
    }

    // Navigation: ; = up, . = down
    if (event.character == ';') {
        _list.scrollUp();
        return true;
    }
    if (event.character == '.') {
        _list.scrollDown();
        return true;
    }

    // Enter — select item
    if (event.enter) {
        int sel = _list.getSelectedIndex();

        if (_subMenu == MENU_MAIN) {
            switch (sel) {
                case 0: _subMenu = MENU_RADIO; buildRadioMenu(); break;
                case 1: _subMenu = MENU_WIFI; buildWiFiMenu(); break;
                case 2: _subMenu = MENU_SDCARD; buildSDCardMenu(); break;
                case 3: _subMenu = MENU_DISPLAY; buildDisplayMenu(); break;
                case 4: _subMenu = MENU_AUDIO; buildAudioMenu(); break;
                case 5: _subMenu = MENU_ABOUT; break;
                case 6: _confirmPending = true; _confirmAction = 0; break;
            }
            return true;
        }

        // WiFi scan results handling
        if (_subMenu == MENU_WIFI_SCAN) {
            int lastItem = _list.itemCount() - 1;
            int rescanItem = lastItem - 1;

            if (sel == lastItem) {
                // "< Back"
                _subMenu = MENU_WIFI;
                buildWiFiMenu();
            } else if (sel == rescanItem) {
                // "[Rescan]"
                startWiFiScan();
            } else if (sel < (int)_scanResults.size()) {
                selectNetwork(sel);
            }
            return true;
        }

        // "Back" is always last item
        if (sel == _list.itemCount() - 1) {
            if (_subMenu == MENU_TCP) {
                _subMenu = MENU_WIFI;
                buildWiFiMenu();
            } else {
                _subMenu = MENU_MAIN;
                buildMainMenu();
            }
            return true;
        }

        // Handle radio presets (items 1..NUM_PRESETS; item 0 is the "Active:" label)
        if (_subMenu == MENU_RADIO && sel >= 1 && sel <= NUM_PRESETS) {
            applyRadioPreset(sel - 1);
            return true;
        }
        // Item 0 ("Active: ...") is non-interactive
        if (_subMenu == MENU_RADIO && sel == 0) {
            return true;
        }

        // Toggle audio on/off (item 0 in Audio menu)
        if (_subMenu == MENU_AUDIO && sel == 0) {
            auto& s = _config->settings();
            s.audioEnabled = !s.audioEnabled;
            if (_audio) _audio->setEnabled(s.audioEnabled);
            applyAndSave();
            buildAudioMenu();
            return true;
        }

        // Cycle WiFi mode (item 0 in WiFi menu)
        if (_subMenu == MENU_WIFI && sel == 0) {
            auto& s = _config->settings();
            s.wifiMode = (RatWiFiMode)(((int)s.wifiMode + 1) % 3);
            applyAndSave();
            showToast("Reboot to apply");
            buildWiFiMenu();
            return true;
        }

        // STA mode WiFi menu actions
        if (_subMenu == MENU_WIFI && _config->settings().wifiMode == RAT_WIFI_STA) {
            if (sel == 1) {
                // Status line (non-interactive)
                return true;
            }
            if (sel == 2) {
                // [Disconnect] or [Connect]
                if (WiFi.status() == WL_CONNECTED) {
                    disconnectWiFi();
                } else {
                    connectWiFi();
                }
                return true;
            }
            if (sel == 3) {
                // [Scan Networks]
                startWiFiScan();
                return true;
            }
            if (sel == 4) {
                // > TCP Connections
                _subMenu = MENU_TCP;
                buildTCPMenu();
                return true;
            }
            if (sel == 5) {
                // Toggle Auto-discover LAN (AutoInterface).  IPv6 enable
                // is one-shot per WiFi init, so changes take effect on
                // next reboot.
                auto& s = _config->settings();
                s.autoIfaceEnabled = !s.autoIfaceEnabled;
                applyAndSave();
                showToast("Reboot to apply");
                buildWiFiMenu();
                return true;
            }
        }

        // SD Card menu actions
        if (_subMenu == MENU_SDCARD) {
            if (_sdStore && _sdStore->isReady()) {
                // Item 4 = Initialize, Item 5 = Wipe All Data
                if (sel == 4) {
                    sdCardFormat();
                    return true;
                }
                if (sel == 5) {
                    _confirmPending = true;
                    _confirmAction = 1;
                    return true;
                }
            }
            // Info items are non-interactive
            return true;
        }

        // TCP submenu actions
        if (_subMenu == MENU_TCP) {
            if (sel == 0) {
                // Add new connection — open host input first
                _editField = 99;
                _editing = true;
                _editLabel = "Host:";
                _editInput.clear();
                _editInput.setActive(true);
                _editInput.setMaxLength(64);
                _editInput.setNumericOnly(false);
                _editInput.setSubmitCallback([this](const std::string& value) {
                    commitEdit(value);
                });
                return true;
            }
            int tcpIdx = sel - 1;
            if (tcpIdx >= 0 && tcpIdx < (int)_config->settings().tcpConnections.size()) {
                toggleTCPConnection(tcpIdx);
                return true;
            }
            return true;  // Back handled above
        }

        // Edit the selected field (offset by 1+NUM_PRESETS for radio header+presets, 1 for WiFi mode)
        int fieldIdx = sel;
        if (_subMenu == MENU_RADIO) fieldIdx -= (1 + NUM_PRESETS);
        if (_subMenu == MENU_WIFI) fieldIdx -= 1;
        std::string currentVal = getCurrentValue(_subMenu, fieldIdx);
        startEditing(fieldIdx, currentVal);
        return true;
    }

    return false;
}

void SettingsScreen::showToast(const char* msg, unsigned long durationMs) {
    _toastMessage = msg;
    _toastUntil = millis() + durationMs;
}

void SettingsScreen::applyAndSave() {
    if (!_config || !_flash) return;

    // Save to both SD + flash when SD is available
    if (_sdStore && _sdStore->isReady()) {
        _config->save(*_sdStore, *_flash);
    } else {
        _config->save(*_flash);
    }

    auto& s = _config->settings();

    // Apply radio settings to hardware
    if (_radio) {
        _radio->setFrequency(s.loraFrequency);
        _radio->setSpreadingFactor(s.loraSF);
        _radio->setSignalBandwidth(s.loraBW);
        _radio->setCodingRate4(s.loraCR);
        _radio->setTxPower(s.loraTxPower);
        _radio->receive();  // Re-enter RX after reconfiguration
    }

    // Apply power settings
    if (_power) {
        _power->setDimTimeout(s.screenDimTimeout);
        _power->setOffTimeout(s.screenOffTimeout);
        _power->setBrightness(s.brightness);
    }

    // Apply audio settings
    if (_audio) {
        _audio->setEnabled(s.audioEnabled);
        _audio->setVolume(s.audioVolume);
    }

    Serial.println("[SETTINGS] Saved and applied");
    if (!_toastMessage) showToast("Saved!");
}

void SettingsScreen::applyRadioPreset(int preset) {
    if (!_config || preset < 0 || preset >= NUM_PRESETS) return;
    auto& s = _config->settings();

    const auto& p = PRESETS[preset];
    s.loraSF = p.sf;
    s.loraBW = p.bw;
    s.loraCR = p.cr;
    s.loraTxPower = p.txPower;
    // Set frequency to current region default
    s.loraFrequency = REGION_FREQ[constrain(s.radioRegion, 0, REGION_COUNT - 1)];

    applyAndSave();
    buildRadioMenu();
    if (_rns) {
        // Encode display name + capability advertisement as msgpack app_data.
        // Format: [display_name(bin), stamp_cost(nil|uint), supported_functionality(array)].
        // stamp_cost=nil means no inbound stamp is required. Empty
        // supported_functionality list signals no SF_COMPRESSION (bz2) support
        // so Python LXMF disables auto_compress for us.
        const String& name = _config ? _config->settings().displayName : String();
        size_t nameLen = name.length();
        if (nameLen > 31) nameLen = 31;
        uint8_t buf[5 + 31];
        size_t i = 0;
        buf[i++] = 0x93;                   // fixarray(3)
        buf[i++] = 0xC4;                   // bin 8
        buf[i++] = (uint8_t)nameLen;
        if (nameLen) { memcpy(buf + i, name.c_str(), nameLen); i += nameLen; }
        buf[i++] = 0xC0;                   // stamp_cost = nil (no stamp required)
        buf[i++] = 0x90;                   // empty fixarray (no SF_* supported)
        _rns->announce(RNS::Bytes(buf, i));
    }
    showToast("Preset applied + announced");
    Serial.printf("[SETTINGS] Radio preset %d applied\n", preset);
}

void SettingsScreen::factoryReset() {
    Serial.println("[SETTINGS] Factory reset — wiping ALL data");

    // 1. Clear ALL NVS namespaces (config, identity, boot counter)
    {
        Preferences prefs;
        if (prefs.begin("ratcom", false)) { prefs.clear(); prefs.end(); }
        if (prefs.begin("ratcom_cfg", false)) { prefs.clear(); prefs.end(); }
        if (prefs.begin("ratcom_id", false)) { prefs.clear(); prefs.end(); }
        Serial.println("[RESET] NVS cleared");
    }

    // 2. Wipe SD card ratcom directory
    if (_sdStore && _sdStore->isReady()) {
        _sdStore->wipeRatcom();
        Serial.println("[RESET] SD wiped");
    }

    // 3. Format LittleFS (destroys all flash files)
    if (_flash) {
        _flash->format();
        Serial.println("[RESET] Flash formatted");
    }

    Serial.println("[RESET] Factory reset complete — rebooting");
    delay(500);
    ESP.restart();
}
