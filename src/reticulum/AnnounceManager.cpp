#include "AnnounceManager.h"
#include "config/Config.h"
#include "storage/SDStore.h"
#include "storage/FlashStore.h"
#include "hal/SharedSPIBus.h"
#include "transport/LoRaInterface.h"
#include "perf/PerfTrace.h"
#include <ArduinoJson.h>
// LittleFS access through FlashStore (mutex-protected)

// Skip one MsgPack value at data[pos], return new pos (or len on error)
static size_t mpSkipValue(const uint8_t* data, size_t len, size_t pos) {
    if (pos >= len) return len;
    uint8_t b = data[pos];
    // positive fixint / negative fixint
    if (b <= 0x7F || b >= 0xE0) return pos + 1;
    // fixmap: skip N*2 elements
    if ((b & 0xF0) == 0x80) {
        size_t n = (b & 0x0F) * 2;
        pos++;
        for (size_t j = 0; j < n && pos < len; j++) pos = mpSkipValue(data, len, pos);
        return pos;
    }
    // fixarray: skip N elements
    if ((b & 0xF0) == 0x90) {
        size_t n = b & 0x0F;
        pos++;
        for (size_t j = 0; j < n && pos < len; j++) pos = mpSkipValue(data, len, pos);
        return pos;
    }
    // fixstr
    if ((b & 0xE0) == 0xA0) return pos + 1 + (b & 0x1F);
    // nil, false, true
    if (b == 0xC0 || b == 0xC2 || b == 0xC3) return pos + 1;
    // bin8
    if (b == 0xC4 && pos + 1 < len) return pos + 2 + data[pos + 1];
    // bin16
    if (b == 0xC5 && pos + 2 < len) return pos + 3 + ((size_t)data[pos + 1] << 8 | data[pos + 2]);
    // float32
    if (b == 0xCA) return pos + 5;
    // float64
    if (b == 0xCB) return pos + 9;
    // uint8, int8
    if (b == 0xCC || b == 0xD0) return pos + 2;
    // uint16, int16
    if (b == 0xCD || b == 0xD1) return pos + 3;
    // uint32, int32
    if (b == 0xCE || b == 0xD2) return pos + 5;
    // uint64, int64
    if (b == 0xCF || b == 0xD3) return pos + 9;
    // str8
    if (b == 0xD9 && pos + 1 < len) return pos + 2 + data[pos + 1];
    // str16
    if (b == 0xDA && pos + 2 < len) return pos + 3 + ((size_t)data[pos + 1] << 8 | data[pos + 2]);
    return len; // unknown type
}

// Try to extract display name from MsgPack-encoded app_data.
// Two-pass: prefer str elements (display name), fall back to bin (NomadNet compat).
static std::string extractMsgPackName(const uint8_t* data, size_t len) {
    if (len < 2) return "";
    uint8_t b = data[0];
    size_t pos = 0;
    size_t arrLen = 0;

    // fixarray (0x90-0x9F)
    if ((b & 0xF0) == 0x90) {
        arrLen = b & 0x0F;
        if (arrLen == 0) return "";
        pos = 1;
    }
    // array16 (0xDC)
    else if (b == 0xDC && len >= 3) {
        arrLen = ((size_t)data[1] << 8) | data[2];
        pos = 3;
    }
    else {
        // Not a MsgPack array — try bare str/bin value (Rust sends raw UTF-8,
        // some implementations might send bare msgpack str or bin)
        size_t slen = 0;
        size_t hdr = 0;
        if ((b & 0xE0) == 0xA0) { slen = b & 0x1F; hdr = 1; }           // fixstr
        else if (b == 0xD9 && len >= 2) { slen = data[1]; hdr = 2; }     // str8
        else if (b == 0xDA && len >= 3) { slen = ((size_t)data[1] << 8) | data[2]; hdr = 3; } // str16
        else if (b == 0xC4 && len >= 2) { slen = data[1]; hdr = 2; }     // bin8
        else if (b == 0xC5 && len >= 3) { slen = ((size_t)data[1] << 8) | data[2]; hdr = 3; } // bin16
        if (hdr > 0 && slen > 0 && hdr + slen <= len) {
            return std::string((const char*)&data[hdr], slen);
        }
        return "";  // Not a recognized MsgPack format
    }

    // Pass 1: scan for first non-empty STR element
    size_t savedPos = pos;
    for (size_t i = 0; i < arrLen && pos < len; i++) {
        b = data[pos];
        size_t slen = 0;
        if ((b & 0xE0) == 0xA0) { slen = b & 0x1F; pos++; }
        else if (b == 0xD9 && pos + 1 < len) { slen = data[pos + 1]; pos += 2; }
        else if (b == 0xDA && pos + 2 < len) { slen = ((size_t)data[pos + 1] << 8) | data[pos + 2]; pos += 3; }
        else { pos = mpSkipValue(data, len, pos); continue; }
        if (slen > 0 && pos + slen <= len) return std::string((const char*)&data[pos], slen);
        pos += slen;
    }

    // Pass 2: scan for first non-empty BIN element (fallback)
    pos = savedPos;
    for (size_t i = 0; i < arrLen && pos < len; i++) {
        b = data[pos];
        size_t slen = 0;
        if (b == 0xC4 && pos + 1 < len) { slen = data[pos + 1]; pos += 2; }
        else if (b == 0xC5 && pos + 2 < len) { slen = ((size_t)data[pos + 1] << 8) | data[pos + 2]; pos += 3; }
        else { pos = mpSkipValue(data, len, pos); continue; }
        if (slen > 0 && pos + slen <= len) return std::string((const char*)&data[pos], slen);
        pos += slen;
    }
    return "";
}

// Character filter — safe displayable characters including UTF-8 multibyte
static std::string sanitizeName(const std::string& raw, size_t maxLen = 32) {
    std::string clean;
    clean.reserve(std::min(raw.size(), maxLen));
    const uint8_t* p = (const uint8_t*)raw.data();
    size_t sz = raw.size();
    for (size_t i = 0; i < sz && clean.size() < maxLen; ) {
        uint8_t c = p[i];
        // ASCII safe chars
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' || c == '-' || c == '_' || c == '.' || c == '\'' || c == '/') {
            clean += (char)c;
            i++;
        }
        // UTF-8 multibyte: pass through valid sequences (display names with emoji/accents)
        else if ((c & 0xE0) == 0xC0 && i + 1 < sz && (p[i+1] & 0xC0) == 0x80) {
            clean.append((const char*)&p[i], 2); i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < sz &&
                 (p[i+1] & 0xC0) == 0x80 && (p[i+2] & 0xC0) == 0x80) {
            clean.append((const char*)&p[i], 3); i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < sz &&
                 (p[i+1] & 0xC0) == 0x80 && (p[i+2] & 0xC0) == 0x80 && (p[i+3] & 0xC0) == 0x80) {
            clean.append((const char*)&p[i], 4); i += 4;
        }
        // Strip control chars, invalid bytes
        else { i++; }
    }
    // Trim leading/trailing spaces
    size_t start = clean.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    size_t end = clean.find_last_not_of(' ');
    return clean.substr(start, end - start + 1);
}

AnnounceManager::AnnounceManager(const char* aspectFilter)
    : RNS::AnnounceHandler(aspectFilter)
{
    _nodes.reserve(MAX_NODES);
    _hashIndex.reserve(MAX_NODES);
}

void AnnounceManager::setStorage(SDStore* sd, FlashStore* flash) {
    _sd = sd;
    _flash = flash;
}

void AnnounceManager::received_announce(
    const RNS::Bytes& destination_hash,
    const RNS::Identity& announced_identity,
    const RNS::Bytes& app_data)
{
    // Extract display name from app_data (do this before filtering so name cache works)
    std::string name;
    if (app_data.size() > 0) {
        Serial.printf("[ANNOUNCE-RX] app_data: %d bytes hex: ", (int)app_data.size());
        for (size_t i = 0; i < std::min((size_t)16, app_data.size()); i++)
            Serial.printf("%02X", app_data.data()[i]);
        Serial.println();
        std::string rawName = extractMsgPackName(app_data.data(), app_data.size());
        if (rawName.empty()) {
            // Use raw bytes as name if valid UTF-8 text (handles Rust raw UTF-8 announces)
            bool isText = app_data.size() > 0 && app_data.size() <= 64;
            const uint8_t* p = app_data.data();
            size_t sz = app_data.size();
            for (size_t i = 0; isText && i < sz; ) {
                uint8_t c = p[i];
                if (c >= 0x20 && c <= 0x7E) { i++; }             // printable ASCII
                else if (c == 0x09) { i++; }                      // tab (ok in names)
                else if ((c & 0xE0) == 0xC0 && i + 1 < sz &&     // 2-byte UTF-8
                         (p[i+1] & 0xC0) == 0x80) { i += 2; }
                else if ((c & 0xF0) == 0xE0 && i + 2 < sz &&     // 3-byte UTF-8
                         (p[i+1] & 0xC0) == 0x80 &&
                         (p[i+2] & 0xC0) == 0x80) { i += 3; }
                else if ((c & 0xF8) == 0xF0 && i + 3 < sz &&     // 4-byte UTF-8
                         (p[i+1] & 0xC0) == 0x80 &&
                         (p[i+2] & 0xC0) == 0x80 &&
                         (p[i+3] & 0xC0) == 0x80) { i += 4; }
                else { isText = false; }
            }
            if (isText) {
                rawName = std::string((const char*)p, sz);
            } else {
                Serial.printf("[ANNOUNCE] Unknown app_data format (%d bytes): ", (int)app_data.size());
                for (size_t i = 0; i < std::min((size_t)32, app_data.size()); i++) {
                    Serial.printf("%02X ", app_data.data()[i]);
                }
                Serial.println();
            }
        }
        name = sanitizeName(rawName);
    }

    // Filter out own announces
    if (_localDestHash.size() > 0 && destination_hash == _localDestHash) return;

    // Layer 3: Global announce rate limit — cap application-layer processing
    {
        unsigned long now = millis();
        if (now - _globalAnnounceWindowStart >= 1000) {
            _globalAnnounceWindowStart = now;
            _globalAnnounceCount = 0;
        }
        if (++_globalAnnounceCount > MAX_GLOBAL_ANNOUNCES_PER_SEC) return;
    }

    // Heap-pressure eviction: aggressively cull non-contacts before processing more
    if (ESP.getFreeHeap() < 25000) {
        evictStale(300000);  // 5 min TTL under pressure (normally 1 hour)
    }

    std::string key = makeKey(destination_hash);
    unsigned long now = millis();

    // O(1) lookup for existing node
    auto it = _hashIndex.find(key);
    if (it != _hashIndex.end()) {
        auto& node = _nodes[it->second];
        if (node.lastSeen != 0 && now - node.lastSeen < ANNOUNCE_MIN_INTERVAL_MS) return;
        if (!name.empty()) {
            node.name = name;
            std::string destHex = destination_hash.toHex();
            _nameCache[destHex] = name;
            while ((int)_nameCache.size() > MAX_NAME_CACHE) {
                _nameCache.erase(_nameCache.begin());
            }
            _nameCacheDirty = true;
        }
        node.lastSeen = now;
        if (_loraIf) { node.rssi = _loraIf->lastRxRssi(); node.snr = _loraIf->lastRxSnr(); }
        if (node.saved) {
            // Contacts: full processing — update hops, persist, cache name
            std::string idHex = announced_identity ? announced_identity.hexhash() : "";
            if (!idHex.empty()) node.identityHex = idHex;
            node.hops = RNS::Transport::hops_to(destination_hash);
            _contactsDirty = true;
        }
        return;
    }

    // New node — lightweight for non-contacts
    std::string destHex = destination_hash.toHex();

    // Make room if full — evict non-contacts first
    if ((int)_nodes.size() >= MAX_NODES) {
        evictStale(600000);  // 10 min TTL for eviction
        if ((int)_nodes.size() >= MAX_NODES) {
            // Find worst non-contact: highest hops, oldest
            int evictIdx = -1;
            uint8_t maxHops = 0;
            unsigned long oldest = ULONG_MAX;
            for (int i = 0; i < (int)_nodes.size(); i++) {
                if (_nodes[i].saved) continue;
                if (_nodes[i].hops > maxHops ||
                    (_nodes[i].hops == maxHops && _nodes[i].lastSeen < oldest)) {
                    maxHops = _nodes[i].hops;
                    oldest = _nodes[i].lastSeen;
                    evictIdx = i;
                }
            }
            if (evictIdx >= 0) {
                int lastIdx = (int)_nodes.size() - 1;
                if (evictIdx != lastIdx) {
                    std::string swapKey = makeKey(_nodes[lastIdx].hash);
                    _hashIndex[swapKey] = evictIdx;
                    std::swap(_nodes[evictIdx], _nodes[lastIdx]);
                }
                _hashIndex.erase(makeKey(_nodes[lastIdx].hash));
                _nodes.pop_back();
            }
        }
    }
    if ((int)_nodes.size() >= MAX_NODES) return;

    DiscoveredNode node;
    node.hash = destination_hash;
    node.name = name.empty() ? destHex.substr(0, 12) : name;
    node.lastSeen = millis();
    if (_loraIf) { node.rssi = _loraIf->lastRxRssi(); node.snr = _loraIf->lastRxSnr(); }
    _hashIndex[key] = (int)_nodes.size();
    _nodes.push_back(node);
    if (!name.empty()) {
        _nameCache[destHex] = name;
        while ((int)_nameCache.size() > MAX_NAME_CACHE) {
            _nameCache.erase(_nameCache.begin());
        }
        _nameCacheDirty = true;
    }
    // Skip identityHex for non-contacts (saves ~64 bytes per node)
}

const DiscoveredNode* AnnounceManager::findNode(const RNS::Bytes& hash) const {
    auto it = _hashIndex.find(makeKey(hash));
    if (it != _hashIndex.end()) return &_nodes[it->second];
    return nullptr;
}

const DiscoveredNode* AnnounceManager::findNodeByHex(const std::string& hexHash) const {
    RNS::Bytes target;
    target.assignHex(hexHash.c_str());
    auto it = _hashIndex.find(makeKey(target));
    if (it != _hashIndex.end()) return &_nodes[it->second];
    // Prefix match fallback (for truncated 16-char conversation hashes)
    for (const auto& n : _nodes) {
        std::string nodeHex = n.hash.toHex();
        if (hexHash.length() < nodeHex.length() &&
            nodeHex.substr(0, hexHash.length()) == hexHash) return &n;
    }
    return nullptr;
}

void AnnounceManager::addManualContact(const std::string& hexHash, const std::string& name) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());

    std::string safeName = sanitizeName(name);
    std::string key = makeKey(hash);

    // Check duplicate via index
    auto it = _hashIndex.find(key);
    if (it != _hashIndex.end()) {
        auto& node = _nodes[it->second];
        if (!safeName.empty()) node.name = safeName;
        node.saved = true;
        saveContact(node);
        return;
    }

    DiscoveredNode node;
    node.hash = hash;
    node.name = safeName.empty() ? hexHash.substr(0, 12) : safeName;
    node.lastSeen = millis();
    node.saved = true;
    _hashIndex[key] = (int)_nodes.size();
    _nodes.push_back(node);
    saveContact(node);
}

void AnnounceManager::saveNode(const std::string& hexHash) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());
    auto it = _hashIndex.find(makeKey(hash));
    if (it != _hashIndex.end()) {
        auto& node = _nodes[it->second];
        node.saved = true;
        saveContact(node);
        // Always persist contact name to name cache
        if (!node.name.empty()) {
            _nameCache[hexHash] = node.name;
            _nameCacheDirty = true;
        }
        Serial.printf("[ANNOUNCE] Saved contact: %s\n", node.name.c_str());
    }
}

void AnnounceManager::unsaveNode(const std::string& hexHash) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());
    auto it = _hashIndex.find(makeKey(hash));
    if (it != _hashIndex.end()) {
        _nodes[it->second].saved = false;
        removeContact(hexHash);
        Serial.printf("[ANNOUNCE] Removed contact: %s\n", _nodes[it->second].name.c_str());
    }
}

void AnnounceManager::evictStale(unsigned long maxAgeMs) {
    unsigned long now = millis();
    _nodes.erase(
        std::remove_if(_nodes.begin(), _nodes.end(),
            [now, maxAgeMs](const DiscoveredNode& n) {
                return !n.saved && n.lastSeen != 0 && (now - n.lastSeen > maxAgeMs);
            }),
        _nodes.end());
    rebuildIndex();
}

void AnnounceManager::clearAll() {
    _nodes.clear();
    _hashIndex.clear();
    _nameCache.clear();
    _contactsDirty = false;
    _nameCacheDirty = false;
    _lastNameCacheSave = 0;
    Serial.println("[ANNOUNCE] Cleared all nodes and name cache");
}

void AnnounceManager::rebuildIndex() {
    _hashIndex.clear();
    for (int i = 0; i < (int)_nodes.size(); i++) {
        _hashIndex[makeKey(_nodes[i].hash)] = i;
    }
}

void AnnounceManager::saveContact(const DiscoveredNode& node) {
    std::string hexHash = node.hash.toHex();

    JsonDocument doc;
    doc["hash"] = hexHash;
    doc["name"] = node.name;

    String json;
    serializeJson(doc, json);

    String filename = hexHash.substr(0, 16).c_str();
    filename += ".json";

    bool sdOk = false, flashOk = false;

    // Write to SD (primary)
    if (_sd && _sd->isReady()) {
        _sd->ensureDir(SD_PATH_CONTACTS);
        String sdPath = String(SD_PATH_CONTACTS) + "/" + filename;
        sdOk = _sd->writeString(sdPath.c_str(), json);
        if (!sdOk) {
            // Atomic rename failed — try direct write
            sdOk = _sd->writeDirect(sdPath.c_str(),
                                     (const uint8_t*)json.c_str(), json.length());
        }
        Serial.printf("[CONTACT] SD write %s: %s\n",
                      sdOk ? "OK" : "FAILED", sdPath.c_str());
    } else {
        Serial.println("[CONTACT] SD not ready, skipping");
    }

    // Write to flash (fallback)
    if (_flash && _flash->isReady()) {
        _flash->ensureDir(PATH_CONTACTS);
        String flashPath = String(PATH_CONTACTS) + "/" + filename;
        flashOk = _flash->writeString(flashPath.c_str(), json);
        if (!flashOk) {
            flashOk = _flash->writeDirect(flashPath.c_str(),
                                           (const uint8_t*)json.c_str(), json.length());
        }
        Serial.printf("[CONTACT] Flash write %s: %s\n",
                      flashOk ? "OK" : "FAILED", flashPath.c_str());
    }

    if (!sdOk && !flashOk) {
        Serial.printf("[CONTACT] WARNING: Failed to persist contact %s!\n",
                      hexHash.substr(0, 8).c_str());
    }
}

void AnnounceManager::removeContact(const std::string& hexHash) {
    String filename = hexHash.substr(0, 16).c_str();
    filename += ".json";

    if (_sd && _sd->isReady()) {
        String sdPath = String(SD_PATH_CONTACTS) + "/" + filename;
        _sd->remove(sdPath.c_str());
        // Also clean up atomic write artifacts
        _sd->remove((sdPath + ".tmp").c_str());
        _sd->remove((sdPath + ".bak").c_str());
    }
    if (_flash && _flash->isReady()) {
        String flashPath = String(PATH_CONTACTS) + "/" + filename;
        _flash->remove(flashPath.c_str());
        _flash->remove((flashPath + ".tmp").c_str());
        _flash->remove((flashPath + ".bak").c_str());
    }
    Serial.printf("[CONTACT] Removed contact %s\n", hexHash.substr(0, 8).c_str());
}

void AnnounceManager::loadContacts() {
    int loaded = 0;

    auto loadFromDir = [&](File& dir, const char* source) {
        int filesFound = 0;
        File entry = dir.openNextFile();
        while (entry) {
            String entryName = entry.name();
            if (!entry.isDirectory() && entryName.endsWith(".json")) {
                filesFound++;
                size_t size = entry.size();
                if (size > 0 && size < 2048) {
                    String json = entry.readString();

                    JsonDocument doc;
                    if (!deserializeJson(doc, json)) {
                        std::string hexHash = doc["hash"] | "";
                        if (!hexHash.empty()) {
                            // Check if already loaded (avoid duplicates)
                            RNS::Bytes hash;
                            hash.assignHex(hexHash.c_str());
                            std::string key = makeKey(hash);
                            if (_hashIndex.find(key) == _hashIndex.end()) {
                                DiscoveredNode node;
                                node.hash = hash;
                                node.name = sanitizeName(doc["name"] | "");
                                if (node.name.empty()) node.name = hexHash.substr(0, 12);
                                node.rssi = 0;
                                node.snr = 0.0f;
                                node.hops = 0;
                                node.lastSeen = 0;
                                node.saved = true;
                                _hashIndex[key] = (int)_nodes.size();
                                _nodes.push_back(node);
                                loaded++;
                                Serial.printf("[CONTACT] Loaded from %s: %s (%s)\n",
                                              source, node.name.c_str(),
                                              hexHash.substr(0, 8).c_str());
                            }
                        }
                    } else {
                        Serial.printf("[CONTACT] JSON parse failed: %s\n", entryName.c_str());
                    }
                }
            }
            entry = dir.openNextFile();
        }
        Serial.printf("[CONTACT] %s: %d json files found\n", source, filesFound);
    };

    // Load from SD first (primary)
    if (_sd && _sd->isReady()) {
        SharedSPILock lock;
        if (lock.locked()) {
            File dir = _sd->openDir(SD_PATH_CONTACTS);
            if (dir && dir.isDirectory()) {
                loadFromDir(dir, "SD");
            } else {
                Serial.println("[CONTACT] SD contacts dir not found");
            }
            dir.close();
        }
    } else {
        Serial.println("[CONTACT] SD not ready for contact loading");
    }

    // Load from flash (any contacts not already on SD)
    if (_flash && _flash->isReady()) {
        File dir = _flash->openDir(PATH_CONTACTS);
        if (dir && dir.isDirectory()) {
            loadFromDir(dir, "Flash");
        } else {
            Serial.println("[CONTACT] Flash contacts dir not found");
        }
    }

    Serial.printf("[CONTACT] Total loaded: %d saved contacts\n", loaded);
}

void AnnounceManager::saveContacts() {
    if (ESP.getFreeHeap() < 20000) {
        Serial.println("[ANNOUNCE] Contact save deferred (low heap)");
        _contactsDirty = true;
        return;
    }
    for (const auto& node : _nodes) {
        if (node.saved) {
            saveContact(node);
            yield();
        }
    }
    _contactsDirty = false;
}

void AnnounceManager::loop() {
    unsigned long now = millis();
    if (_contactsDirty && now - _lastContactSave >= CONTACT_SAVE_INTERVAL_MS) {
        _contactsDirty = false;
        _lastContactSave = now;
        saveContacts();
        Serial.println("[ANNOUNCE] Deferred contact save complete");
    }
    if (_nameCacheDirty && now - _lastNameCacheSave >= NAME_CACHE_SAVE_INTERVAL_MS) {
        _nameCacheDirty = false;
        _lastNameCacheSave = now;
        saveNameCache();
    }
}

std::string AnnounceManager::lookupName(const std::string& hexHash) const {
    // Check live nodes first
    const DiscoveredNode* node = findNodeByHex(hexHash);
    if (node && !node->name.empty()) return node->name;
    // Fall back to cached names
    auto it = _nameCache.find(hexHash);
    if (it != _nameCache.end()) return it->second;
    for (const auto& kv : _nameCache) {
        if (hexHash.length() < kv.first.length() &&
            kv.first.substr(0, hexHash.length()) == hexHash) {
            return kv.second;
        }
    }
    return "";
}

void AnnounceManager::saveNameCache() {
    unsigned long traceStart = PerfTrace::nowMs();
    // Skip save when heap is low — the JsonDocument + String allocation is expensive
    if (ESP.getFreeHeap() < 20000) {
        Serial.println("[ANNOUNCE] Name cache save deferred (low heap)");
        _nameCacheDirty = true;  // Retry later
        PerfTrace::log("announce", "save_name_cache", "deferred", "/config/names.json",
                       0, (int)_nameCache.size(), false, PerfTrace::elapsedMs(traceStart));
        return;
    }
    JsonDocument doc;
    for (auto& kv : _nameCache) {
        doc[kv.first] = kv.second;
    }
    String json;
    serializeJson(doc, json);
    bool sdReady = _sd && _sd->isReady();
    bool flashReady = _flash && _flash->isReady();
    bool sdOk = false;
    bool flashOk = false;
    if (_sd && _sd->isReady()) {
        _sd->ensureDir(SD_PATH_CONFIG_DIR);
        sdOk = _sd->writeString("/ratcom/config/names.json", json);
    }
    if (_flash && _flash->isReady()) {
        _flash->ensureDir("/config");
        flashOk = _flash->writeString("/config/names.json", json);
    }
    const char* backend = sdReady && flashReady ? "both" : (sdReady ? "sd" : (flashReady ? "flash" : "none"));
    bool ok = (sdReady && sdOk) || (flashReady && flashOk);
    PerfTrace::log("announce", "save_name_cache", backend, "/config/names.json",
                   json.length(), (int)_nameCache.size(), ok, PerfTrace::elapsedMs(traceStart));
    Serial.printf("[ANNOUNCE] Name cache saved (%d entries)\n", (int)_nameCache.size());
}

void AnnounceManager::loadNameCache() {
    String json;
    if (_sd && _sd->isReady()) {
        json = _sd->readString("/ratcom/config/names.json");
    }
    if (json.isEmpty() && _flash && _flash->isReady()) {
        json = _flash->readString("/config/names.json");
    }
    if (json.isEmpty()) return;
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    for (JsonPair kv : doc.as<JsonObject>()) {
        _nameCache[kv.key().c_str()] = kv.value().as<std::string>();
    }
    Serial.printf("[ANNOUNCE] Name cache loaded (%d entries)\n", (int)_nameCache.size());
}
