#include "NodesScreen.h"
#include "ui/Theme.h"
#include <algorithm>

void NodesScreen::onEnter() {
    refreshList();
}

void NodesScreen::onExit() {
    _showingActions = false;
}

// Helper: truncate name to maxChars respecting UTF-8 boundaries
static std::string truncUTF8(const std::string& name, size_t maxChars) {
    size_t chars = 0, i = 0;
    const uint8_t* p = (const uint8_t*)name.data();
    size_t sz = name.size();
    while (i < sz && chars < maxChars) {
        uint8_t c = p[i];
        size_t seqLen = 1;
        if ((c & 0xE0) == 0xC0) seqLen = 2;
        else if ((c & 0xF0) == 0xE0) seqLen = 3;
        else if ((c & 0xF8) == 0xF0) seqLen = 4;
        if (i + seqLen > sz) break;
        i += seqLen;
        chars++;
    }
    return name.substr(0, i);
}

// Helper: format a node line
static void formatNodeLine(char* line, size_t lineSize, const DiscoveredNode& node) {
    std::string displayName = truncUTF8(node.name, 18);
    if (node.lastSeen == 0) {
        snprintf(line, lineSize, "%-20s saved", displayName.c_str());
        return;
    }
    unsigned long ago = (millis() - node.lastSeen) / 1000;
    if (ago < 60) {
        if (node.hops < 128)
            snprintf(line, lineSize, "%-20s %3lus %dhop", displayName.c_str(), ago, node.hops);
        else
            snprintf(line, lineSize, "%-20s %3lus", displayName.c_str(), ago);
    } else {
        if (node.hops < 128)
            snprintf(line, lineSize, "%-20s %3lum %dhop", displayName.c_str(), ago / 60, node.hops);
        else
            snprintf(line, lineSize, "%-20s %3lum", displayName.c_str(), ago / 60);
    }
}

// Helper: staleness color for a node
static uint16_t stalenessColor(const DiscoveredNode& node) {
    if (node.lastSeen == 0) return Theme::MUTED;
    unsigned long ageMs = millis() - node.lastSeen;
    if (ageMs > 1800000) return Theme::MUTED;
    if (ageMs > 300000) return Theme::BORDER + 0x0220;
    return 0;  // default
}

void NodesScreen::refreshList() {
    if (!_announces) return;

    std::string prevSelected;
    int oldIdx = _list.getSelectedIndex();
    if (oldIdx >= 0 && oldIdx < (int)_nodeHashes.size()) {
        prevSelected = _nodeHashes[oldIdx];
    }

    _list.clear();
    _nodeHashes.clear();
    _contactCount = 0;

    const auto& nodes = _announces->nodes();

    // Separate contacts and peers, sort each alphabetically
    struct SortEntry { int idx; std::string lower; };
    std::vector<SortEntry> contacts, peers;

    for (int i = 0; i < (int)nodes.size(); i++) {
        std::string l = nodes[i].name;
        std::transform(l.begin(), l.end(), l.begin(), ::tolower);
        if (nodes[i].saved) {
            contacts.push_back({i, std::move(l)});
        } else {
            peers.push_back({i, std::move(l)});
        }
    }

    // Sort: letters before numbers, then alphabetical
    auto sortFn = [](const SortEntry& a, const SortEntry& b) {
        bool aDigit = !a.lower.empty() && a.lower[0] >= '0' && a.lower[0] <= '9';
        bool bDigit = !b.lower.empty() && b.lower[0] >= '0' && b.lower[0] <= '9';
        if (aDigit != bDigit) return bDigit;
        return a.lower < b.lower;
    };
    std::sort(contacts.begin(), contacts.end(), sortFn);
    std::sort(peers.begin(), peers.end(), sortFn);

    _lastKnownCount = (int)nodes.size();
    int newSelectedIdx = 0;

    // === Contacts section (only if contacts exist) ===
    if (!contacts.empty()) {
        for (auto& entry : contacts) {
            const auto& node = nodes[entry.idx];
            char line[64];
            formatNodeLine(line, sizeof(line), node);
            _list.addItem(line, stalenessColor(node));
            std::string hexStr = node.hash.toHex();
            _nodeHashes.push_back(hexStr);
            if (!prevSelected.empty() && hexStr == prevSelected) {
                newSelectedIdx = (int)_nodeHashes.size() - 1;
            }
        }
        _contactCount = (int)contacts.size();

        // Separator between contacts and peers
        if (!peers.empty()) {
            char sep[32];
            snprintf(sep, sizeof(sep), "-- Peers (%d) --", (int)peers.size());
            _list.addItem(sep, Theme::MUTED);
            _nodeHashes.push_back("");  // not selectable
        }
    }

    // === Peers section (always shown) ===
    for (auto& entry : peers) {
        const auto& node = nodes[entry.idx];
        char line[64];
        formatNodeLine(line, sizeof(line), node);
        _list.addItem(line, stalenessColor(node));
        std::string hexStr = node.hash.toHex();
        _nodeHashes.push_back(hexStr);
        if (!prevSelected.empty() && hexStr == prevSelected) {
            newSelectedIdx = (int)_nodeHashes.size() - 1;
        }
    }

    if (!_nodeHashes.empty()) {
        // Skip section headers when restoring selection
        if (newSelectedIdx == 0 && _nodeHashes.size() > 1) newSelectedIdx = 1;
        _list.setSelected(newSelectedIdx);
    }

    _lastRefresh = millis();
}

void NodesScreen::showActionMenu(int nodeIdx) {
    if (!_announces || nodeIdx < 0 || nodeIdx >= (int)_nodeHashes.size()) return;
    if (_nodeHashes[nodeIdx].empty()) return;  // Section header

    _selectedNodeIdx = nodeIdx;
    _selectedNodeHash = _nodeHashes[nodeIdx];

    // Try to find live node — may have been evicted since list was built
    RNS::Bytes hash;
    hash.assignHex(_selectedNodeHash.c_str());
    const DiscoveredNode* node = _announces->findNode(hash);

    if (node) {
        _selectedNodeName = node->name;
        _selectedNodeSaved = node->saved;
    } else {
        // Node was evicted — use name from the list item text, mark as not saved
        _selectedNodeName = _selectedNodeHash.substr(0, 12);
        // Try name cache as fallback
        std::string cached = _announces->lookupName(_selectedNodeHash);
        if (!cached.empty()) _selectedNodeName = cached;
        _selectedNodeSaved = false;
    }

    _actionList.clear();
    _actionList.addItem("Message");
    if (node) {
        if (_selectedNodeSaved) {
            _actionList.addItem("Remove Contact");
        } else {
            _actionList.addItem("Save Contact");
        }
    }
    _actionList.addItem("Back");
    _actionList.setSelected(0);

    _showingActions = true;
}

void NodesScreen::executeAction(int actionIdx) {
    const std::string& action = _actionList.getSelectedItem();

    if (action == "Message") {
        exitActionMenu();
        if (_selectCb) {
            _selectCb(_selectedNodeHash);
        }
    } else if (action == "Save Contact") {
        if (_saveCb) _saveCb(_selectedNodeHash, true);
        exitActionMenu();
        refreshList();
    } else if (action == "Remove Contact") {
        if (_saveCb) _saveCb(_selectedNodeHash, false);
        exitActionMenu();
        refreshList();
    } else {
        exitActionMenu();
    }
}

void NodesScreen::exitActionMenu() {
    _showingActions = false;
    _selectedNodeIdx = -1;
}

void NodesScreen::render(M5Canvas& canvas) {
    if (!_showingActions && millis() - _lastRefresh > 30000) {
        int delta = abs(_announces->nodeCount() - _lastKnownCount);
        if (delta > 0) refreshList();
    }

    int y = Theme::CONTENT_Y;

    if (_showingActions) {
        const int headerH = 17;
        canvas.fillRect(0, y, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
        canvas.fillRect(0, y + 2, 3, headerH - 4, Theme::PRIMARY);
        Theme::useUiFont(canvas);
        canvas.setTextColor(Theme::TEXT_PRIMARY);
        {
            char truncName[80];
            size_t chars = 0, i = 0;
            const uint8_t* p = (const uint8_t*)_selectedNodeName.data();
            size_t sz = _selectedNodeName.size();
            while (i < sz && chars < 26 && i < sizeof(truncName) - 1) {
                uint8_t c = p[i];
                size_t seqLen = 1;
                if ((c & 0xE0) == 0xC0) seqLen = 2;
                else if ((c & 0xF0) == 0xE0) seqLen = 3;
                else if ((c & 0xF8) == 0xF0) seqLen = 4;
                if (i + seqLen > sz || i + seqLen > sizeof(truncName) - 1) break;
                i += seqLen;
                chars++;
            }
            memcpy(truncName, _selectedNodeName.data(), i);
            truncName[i] = '\0';
            canvas.drawString(truncName, 8, y + 2);
        }
        y += headerH + 2;
        canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::DIVIDER);
        y += 2;

        _actionList.render(canvas, 0, y, Theme::SCREEN_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
    } else {
        const int headerH = 17;
        canvas.fillRect(0, y, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
        canvas.fillRect(0, y + 2, 3, headerH - 4, Theme::ACCENT);
        Theme::useUiFont(canvas);
        canvas.setTextColor(Theme::ACCENT);
        canvas.setCursor(8, y + 2);
        if (_contactCount > 0) {
            canvas.printf("Contacts (%d)", _contactCount);
        } else {
            canvas.printf("Peers (%d)", _announces ? _announces->nodeCount() : 0);
        }

        canvas.drawFastHLine(0, y + headerH, Theme::SCREEN_W, Theme::DIVIDER);
        y += headerH + 2;

        if (_list.itemCount() == 0) {
            Theme::useSmallFont(canvas);
            canvas.setTextColor(Theme::MUTED);
            canvas.setCursor(4, y + 10);
            canvas.print("No nodes discovered yet.");
            canvas.setCursor(4, y + 22);
            canvas.print("Waiting for announces...");
        } else {
            _list.render(canvas, 0, y, Theme::SCREEN_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
        }
    }
    Theme::useSmallFont(canvas);
}

bool NodesScreen::handleKey(const KeyEvent& event) {
    if (_showingActions) {
        // ESC or Delete exits action menu
        if (event.character == 27 || event.del) {
            exitActionMenu();
            return true;
        }
        if (event.character == ';') {
            _actionList.scrollUp();
            return true;
        }
        if (event.character == '.') {
            _actionList.scrollDown();
            return true;
        }
        if (event.enter) {
            executeAction(_actionList.getSelectedIndex());
            return true;
        }
        return true;
    }

    // Navigation
    if (event.character == ';') {
        _list.scrollUp();
        // Skip section headers
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_nodeHashes.size() && _nodeHashes[idx].empty()) {
            _list.scrollUp();
        }
        return true;
    }
    if (event.character == '.') {
        _list.scrollDown();
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_nodeHashes.size() && _nodeHashes[idx].empty()) {
            _list.scrollDown();
        }
        return true;
    }
    if (event.enter) {
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_nodeHashes.size() && !_nodeHashes[idx].empty()) {
            showActionMenu(idx);
        }
        return true;
    }
    return false;
}
