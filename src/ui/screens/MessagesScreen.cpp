#include "MessagesScreen.h"
#include "ui/Theme.h"
#include "reticulum/AnnounceManager.h"

void MessagesScreen::onEnter() {
    _showingContext = false;
    refreshList();
}

void MessagesScreen::refreshList() {
    if (!_lxmf) return;

    _list.clear();
    _peerHexes.clear();

    const auto& convs = _lxmf->conversations();
    for (const auto& peerHex : convs) {
        std::string label;
        if (_am) {
            const DiscoveredNode* node = _am->findNodeByHex(peerHex);
            if (node && !node->name.empty()) {
                label = node->name;
            }
        }
        if (label.empty()) {
            if (peerHex.size() >= 8) {
                label = peerHex.substr(0, 4) + ":" + peerHex.substr(4, 4);
            } else {
                label = peerHex;
            }
        }

        int unread = _lxmf->unreadCount(peerHex);
        if (unread > 0) {
            label += " [" + std::to_string(unread) + "]";
        }

        _list.addItem(label);
        _peerHexes.push_back(peerHex);
    }

    if (_list.itemCount() == 0) {
        _list.addItem("No conversations yet");
    }

    _lastRefresh = millis();
    _needsRefresh = false;
}

void MessagesScreen::showContextMenu(int idx) {
    if (idx < 0 || idx >= (int)_peerHexes.size()) return;
    _contextPeerHex = _peerHexes[idx];

    _contextIsContact = false;
    if (_am) {
        const DiscoveredNode* node = _am->findNodeByHex(_contextPeerHex);
        if (node && node->saved) _contextIsContact = true;
    }

    _contextList.clear();
    _contextList.addItem("Message");
    if (!_contextIsContact) {
        _contextList.addItem("Add Contact");
    }
    _contextList.addItem("Delete History");
    _contextList.addItem("Back");
    _contextList.setSelected(0);
    _showingContext = true;
}

void MessagesScreen::executeContextAction() {
    const std::string& action = _contextList.getSelectedItem();

    if (action == "Message") {
        exitContextMenu();
        if (_openCb) _openCb(_contextPeerHex);
    } else if (action == "Add Contact") {
        if (_addContactCb) _addContactCb(_contextPeerHex);
        exitContextMenu();
    } else if (action == "Delete History") {
        if (_lxmf) {
            _lxmf->deleteConversation(_contextPeerHex);
        }
        exitContextMenu();
        refreshList();
    } else {
        exitContextMenu();
    }
}

void MessagesScreen::exitContextMenu() {
    _showingContext = false;
    _contextPeerHex.clear();
}

void MessagesScreen::render(M5Canvas& canvas) {
    if (_needsRefresh) {
        refreshList();
    }

    int y = Theme::CONTENT_Y;

    const int headerH = 17;
    canvas.fillRect(0, y, Theme::CONTENT_W, headerH, Theme::BG_SURFACE);
    canvas.fillRect(0, y + 2, 3, headerH - 4, Theme::ACCENT);
    canvas.setTextColor(Theme::ACCENT);
    Theme::useUiFont(canvas);
    canvas.drawString("Messages", 8, y + 2);
    canvas.drawFastHLine(0, y + headerH, Theme::CONTENT_W, Theme::DIVIDER);
    y += headerH + 2;

    if (_showingContext) {
        Theme::useUiFont(canvas);
        canvas.setTextColor(Theme::PRIMARY);
        std::string label;
        if (_am) {
            const DiscoveredNode* node = _am->findNodeByHex(_contextPeerHex);
            if (node && !node->name.empty()) label = node->name;
        }
        if (label.empty()) label = _contextPeerHex.substr(0, 8);
        canvas.drawString(label.c_str(), 8, y);
        y += Theme::LIST_ROW_H + 2;
        canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::DIVIDER);
        y += 2;

        _contextList.render(canvas, 0, y, Theme::CONTENT_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
    } else {
        _list.render(canvas, 0, y, Theme::CONTENT_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
    }
    Theme::useSmallFont(canvas);
}

bool MessagesScreen::handleKey(const KeyEvent& event) {
    if (_showingContext) {
        if (event.character == 27 || event.del) {
            exitContextMenu();
            return true;
        }
        if (event.character == ';') { _contextList.scrollUp(); return true; }
        if (event.character == '.') { _contextList.scrollDown(); return true; }
        if (event.enter) {
            executeContextAction();
            return true;
        }
        return true;
    }

    if (event.character == ';') { _list.scrollUp(); return true; }
    if (event.character == '.') { _list.scrollDown(); return true; }

    // Enter → open conversation immediately
    if (event.enter) {
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_peerHexes.size() && _openCb) {
            _openCb(_peerHexes[idx]);
        }
        return true;
    }

    // Fn+Enter or Delete key → context menu (Message, Add Contact, Delete History)
    if (event.del) {
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_peerHexes.size()) {
            showContextMenu(idx);
        }
        return true;
    }

    return false;
}
