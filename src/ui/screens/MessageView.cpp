#include "MessageView.h"
#include "ui/Theme.h"
#include "reticulum/AnnounceManager.h"
#include <algorithm>
#include <time.h>

namespace {
constexpr int CHAT_HEADER_H = 17;
constexpr int CHAT_INPUT_H = Theme::CHAR_H + 4;

int visibleChatLines() {
    int chatH = Theme::CONTENT_H - CHAT_HEADER_H - CHAT_INPUT_H - 4;
    return chatH > 0 ? chatH / Theme::CHAR_H : 1;
}

void drawFittedHeader(M5Canvas& canvas, const std::string& text, int x, int y, int maxW) {
    if (canvas.textWidth(text.c_str()) <= maxW) {
        canvas.drawString(text.c_str(), x, y);
        return;
    }
    char buf[64];
    int len = std::min((int)text.length(), (int)sizeof(buf) - 3);
    while (len > 0) {
        memcpy(buf, text.c_str(), len);
        buf[len] = '.';
        buf[len + 1] = '.';
        buf[len + 2] = '\0';
        if (canvas.textWidth(buf) <= maxW) {
            canvas.drawString(buf, x, y);
            return;
        }
        len--;
    }
}
}

void MessageView::onEnter() {
    _input.setActive(true);
    _input.clear();
    _input.setMaxLength(400);
    _input.setSubmitCallback([this](const std::string& text) {
        sendCurrentInput();
    });
    refreshMessages();
}

void MessageView::refreshMessages() {
    if (!_lxmf || _peerHex.empty()) return;

    // Load last 20 messages (paginated)
    _messages = _lxmf->getMessages(_peerHex);

    // Merge any pending messages not yet flushed to disk.
    // Only remove pending entries that were confirmed written (found in disk load).
    // This prevents messages from vanishing if the WriteQueue hasn't flushed yet.
    auto pendIt = _pendingMessages.find(_peerHex);
    if (pendIt != _pendingMessages.end()) {
        auto& pendings = pendIt->second;
        // _messages currently holds only disk-loaded messages (from getMessages above).
        // Check each pending against disk; add if missing, mark confirmed if found.
        const auto diskMessages = _messages;  // snapshot before merge
        for (const auto& pending : pendings) {
            bool onDisk = false;
            for (const auto& loaded : diskMessages) {
                if (loaded.timestamp == pending.timestamp &&
                    loaded.sourceHash == pending.sourceHash) {
                    onDisk = true; break;
                }
            }
            if (!onDisk) _messages.push_back(pending);
        }
        // Remove only confirmed (on-disk) pending entries; keep unwritten ones
        pendings.erase(std::remove_if(pendings.begin(), pendings.end(),
            [&diskMessages](const LXMFMessage& pending) {
                for (const auto& loaded : diskMessages) {
                    if (loaded.timestamp == pending.timestamp &&
                        loaded.sourceHash == pending.sourceHash) return true;
                }
                return false;
            }), pendings.end());
        if (pendings.empty()) _pendingMessages.erase(pendIt);
    }

    _lxmf->markRead(_peerHex);
    if (_unreadCb) _unreadCb();

    // Build chat lines (IRC-style)
    _chatLines.clear();
    for (const auto& msg : _messages) {
        ChatLine line;

        // Smart timestamp: real HH:MM if epoch, relative if uptime (capped at 24h)
        char ts[16];
        double tsVal = msg.timestamp;
        if (tsVal > 1700000000) {
            time_t t = (time_t)tsVal;
            struct tm* tm = localtime(&t);
            snprintf(ts, sizeof(ts), "%02d:%02d", tm->tm_hour, tm->tm_min);
        } else if (tsVal > 0 && (millis() / 1000) > (unsigned long)tsVal) {
            unsigned long ago = (millis() / 1000) - (unsigned long)tsVal;
            if (ago < 60) snprintf(ts, sizeof(ts), "%lus", ago);
            else if (ago < 3600) snprintf(ts, sizeof(ts), "%lum", ago / 60);
            else if (ago < 86400) snprintf(ts, sizeof(ts), "%luh", ago / 3600);
            else snprintf(ts, sizeof(ts), "--:--");
        } else {
            snprintf(ts, sizeof(ts), "--:--");
        }

        if (msg.incoming) {
            std::string sender = peerDisplayName();
            line.text = std::string(ts) + " " + sender + "> " + msg.content;
            line.color = Theme::PRIMARY;
        } else {
            line.text = std::string(ts) + " you> " + msg.content;
            line.color = (msg.status == LXMFStatus::FAILED) ? Theme::ERROR : Theme::PRIMARY;
        }

        _chatLines.push_back(line);
    }

    // Auto-scroll to bottom
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int totalWrappedLines = 0;
    for (const auto& cl : _chatLines) {
        totalWrappedLines += ((int)cl.text.size() / maxCharsPerLine) + 1;
    }
    int visibleLines = visibleChatLines();
    _scrollOffset = std::max(0, totalWrappedLines - visibleLines);

    _lastRefresh = millis();
    _needsRefresh = false;
}

void MessageView::render(M5Canvas& canvas) {
    // Event-driven refresh instead of timer-based
    if (_needsRefresh) {
        refreshMessages();
    }

    int baseY = Theme::CONTENT_Y;

    // Header: node name or peer hash + back hint
    std::string header;
    if (_am) {
        const DiscoveredNode* node = _am->findNodeByHex(_peerHex);
        if (node && !node->name.empty()) header = node->name;
    }
    if (header.empty()) {
        if (_peerHex.size() >= 8) {
            header = _peerHex.substr(0, 4) + ":" + _peerHex.substr(4, 4);
        } else {
            header = _peerHex;
        }
    }
    canvas.fillRect(0, baseY, Theme::CONTENT_W, CHAT_HEADER_H, Theme::BG_SURFACE);
    canvas.fillRect(0, baseY + 2, 3, CHAT_HEADER_H - 4, Theme::PRIMARY);
    Theme::useUiFont(canvas);
    canvas.setTextColor(Theme::TEXT_PRIMARY);
    drawFittedHeader(canvas, header, 8, baseY + 2, Theme::CONTENT_W - 16);
    canvas.drawFastHLine(0, baseY + CHAT_HEADER_H, Theme::CONTENT_W, Theme::DIVIDER);

    // Chat area
    Theme::useSmallFont(canvas);
    int chatY = baseY + CHAT_HEADER_H + 2;
    int inputY = baseY + Theme::CONTENT_H - CHAT_INPUT_H;
    int chatH = inputY - chatY - 2;
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int currentLine = 0;

    for (const auto& cl : _chatLines) {
        int lineLen = cl.text.size();
        int pos = 0;
        while (pos < lineLen) {
            int remaining = lineLen - pos;
            int chars = std::min(remaining, maxCharsPerLine);

            if (currentLine >= _scrollOffset) {
                int drawY = chatY + (currentLine - _scrollOffset) * Theme::CHAR_H;
                if (drawY + Theme::CHAR_H > chatY + chatH) break;

                canvas.setTextColor(cl.color);
                // Stack buffer avoids heap allocation per line per frame
                char segment[48];
                int segLen = std::min(chars, (int)sizeof(segment) - 1);
                memcpy(segment, cl.text.c_str() + pos, segLen);
                segment[segLen] = '\0';
                canvas.drawString(segment, 2, drawY);
            }

            pos += chars;
            currentLine++;
        }
    }

    // "New message below" indicator when scrolled up
    if (_hasNewBelow) {
        canvas.setTextColor(Theme::WARNING);
        canvas.drawString("[new msg]", Theme::CONTENT_W - 54, chatY + chatH - Theme::CHAR_H);
    }

    // Input separator
    canvas.drawFastHLine(0, inputY - 2, Theme::CONTENT_W, Theme::DIVIDER);

    // Text input
    _input.render(canvas, 0, inputY, Theme::CONTENT_W);
}

bool MessageView::handleKey(const KeyEvent& event) {
    // Escape = back to messages list
    if (event.character == 27) {
        if (_backCb) _backCb();
        return true;
    }

    // Fn+Delete = back (even when text input is active)
    if (event.fn && event.del) {
        if (_backCb) _backCb();
        return true;
    }

    // Delete key = back ONLY if text input is empty
    if (event.del && !event.fn && _input.getText().empty()) {
        if (_backCb) _backCb();
        return true;
    }

    // Pass to text input
    if (_input.handleKey(event)) {
        return true;
    }

    return false;
}

std::string MessageView::peerDisplayName() const {
    if (_am) {
        const DiscoveredNode* node = _am->findNodeByHex(_peerHex);
        if (node && !node->name.empty()) return node->name;
    }
    return _peerHex.size() >= 4 ? _peerHex.substr(0, 4) : _peerHex;
}

void MessageView::notifyNewMessage(const LXMFMessage& msg) {
    std::string senderHex = msg.incoming ?
        msg.sourceHash.toHex() : msg.destHash.toHex();

    // Check if this message is for the peer we're currently viewing
    bool match = (senderHex == _peerHex);
    if (!match && _peerHex.size() < senderHex.size()) {
        match = (senderHex.substr(0, _peerHex.size()) == _peerHex);
    }

    // Always store in pending for merge on next refreshMessages()
    // This ensures messages survive the async write delay
    _pendingMessages[senderHex].push_back(msg);

    if (!match) {
        _needsRefresh = true;
        return;
    }

    // Add directly to chat lines for real-time display
    ChatLine line;
    char ts[16];
    double tsVal = msg.timestamp;
    if (tsVal > 1700000000) {
        time_t t = (time_t)tsVal;
        struct tm* tm = localtime(&t);
        snprintf(ts, sizeof(ts), "%02d:%02d", tm->tm_hour, tm->tm_min);
    } else if (tsVal > 0 && (millis() / 1000) > (unsigned long)tsVal) {
        unsigned long ago = (millis() / 1000) - (unsigned long)tsVal;
        if (ago < 60) snprintf(ts, sizeof(ts), "%lus", ago);
        else if (ago < 3600) snprintf(ts, sizeof(ts), "%lum", ago / 60);
        else if (ago < 86400) snprintf(ts, sizeof(ts), "%luh", ago / 3600);
        else snprintf(ts, sizeof(ts), "--:--");
    } else {
        snprintf(ts, sizeof(ts), "--:--");
    }

    if (msg.incoming) {
        std::string sender = peerDisplayName();
        line.text = std::string(ts) + " " + sender + "> " + msg.content;
        line.color = Theme::PRIMARY;
    } else {
        line.text = std::string(ts) + " you> " + msg.content;
        line.color = Theme::PRIMARY;
    }
    _chatLines.push_back(line);

    // Smart auto-scroll: only scroll if user is already at/near bottom
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int totalWrappedLines = 0;
    for (const auto& cl : _chatLines) {
        totalWrappedLines += ((int)cl.text.size() / maxCharsPerLine) + 1;
    }
    int visibleLines = visibleChatLines();
    int maxScroll = std::max(0, totalWrappedLines - visibleLines);
    if (_scrollOffset >= maxScroll - 2) {
        // User is at/near bottom — auto-scroll
        _scrollOffset = maxScroll;
        _hasNewBelow = false;
    } else {
        // User is reading history — don't scroll, show indicator
        _hasNewBelow = true;
    }

    if (msg.incoming && _lxmf) {
        _lxmf->markRead(_peerHex);
        if (_unreadCb) _unreadCb();
    }
}

void MessageView::notifyStatusChange(const std::string& peerHex, double timestamp, LXMFStatus status) {
    if (peerHex != _peerHex) return;

    // Update the most recent pending outgoing chat line
    for (int i = _chatLines.size() - 1; i >= 0; i--) {
        auto& line = _chatLines[i];
        if (line.text.find("you>") != std::string::npos && line.color == Theme::WARNING) {
            switch (status) {
                case LXMFStatus::SENT:
                case LXMFStatus::DELIVERED:
                    line.color = Theme::PRIMARY;
                    break;
                case LXMFStatus::FAILED:
                    line.color = Theme::ERROR;
                    break;
                default:
                    break;
            }
            break;
        }
    }
}

void MessageView::sendCurrentInput() {
    if (!_lxmf || _peerHex.empty()) return;

    std::string text = _input.getText();
    if (text.empty()) return;

    RNS::Bytes destHash;
    destHash.assignHex(_peerHex.c_str());

    if (!_lxmf->sendMessage(destHash, text)) {
        return;
    }
    _input.clear();

    // Store outgoing message in pending so it survives refresh before disk write completes
    {
        LXMFMessage pending;
        pending.destHash = destHash;
        pending.sourceHash = _lxmf->getSourceHash();
        pending.content = text;
        pending.timestamp = (time(nullptr) > 1700000000) ? (double)time(nullptr) : millis() / 1000.0;
        pending.incoming = false;
        pending.status = LXMFStatus::QUEUED;
        _pendingMessages[_peerHex].push_back(pending);
    }

    // Add sent message to display immediately
    char ts[16];
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm* tm = localtime(&now);
        snprintf(ts, sizeof(ts), "%02d:%02d", tm->tm_hour, tm->tm_min);
    } else {
        snprintf(ts, sizeof(ts), "0s");
    }

    ChatLine line;
    line.text = std::string(ts) + " you> " + text;
    line.color = Theme::PRIMARY;
    _chatLines.push_back(line);

    // Always scroll to bottom when sending
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int totalWrappedLines = 0;
    for (const auto& cl : _chatLines) {
        totalWrappedLines += ((int)cl.text.size() / maxCharsPerLine) + 1;
    }
    int visibleLines = visibleChatLines();
    _scrollOffset = std::max(0, totalWrappedLines - visibleLines);
    _hasNewBelow = false;
}
