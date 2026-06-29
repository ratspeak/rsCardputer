#include "ReticulumManager.h"
#include "config/Config.h"
#include "storage/FlashStore.h"
#include "perf/PerfTrace.h"
#include <Log.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <map>
#include <unordered_map>
#include <string>
#include <utility>

// RAII lock guard for the shared LittleFS mutex
struct FSLock {
    FSLock() : _m(FlashStore::mutex()) {
        if (_m) xSemaphoreTake(_m, portMAX_DELAY);
    }
    ~FSLock() {
        if (_m) xSemaphoreGive(_m);
    }
    SemaphoreHandle_t _m;
};

// =============================================================================
// LittleFS Filesystem Implementation for microReticulum
// =============================================================================

bool LittleFSFileSystem::init() {
    return true;  // LittleFS already initialized by FlashStore
}

bool LittleFSFileSystem::file_exists(const char* file_path) {
    FSLock lock;
    return LittleFS.exists(file_path);
}

size_t LittleFSFileSystem::read_file(const char* file_path, RNS::Bytes& data) {
    FSLock lock;
    File f = LittleFS.open(file_path, "r");
    if (!f) return 0;
    size_t size = f.size();
    data = RNS::Bytes(size);
    f.readBytes((char*)data.writable(size), size);
    f.close();
    return size;
}

size_t LittleFSFileSystem::write_file(const char* file_path, const RNS::Bytes& data) {
    FSLock lock;
    // Ensure parent directory exists
    String path = String(file_path);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = path.substring(0, lastSlash);
        if (!LittleFS.exists(dir.c_str())) {
            LittleFS.mkdir(dir.c_str());
        }
    }

    File f = LittleFS.open(file_path, "w");
    if (!f) return 0;
    size_t written = f.write(data.data(), data.size());
    f.close();
    return written;
}

namespace {
const char* persistCycleName(uint8_t cycle) {
    switch (cycle) {
        case 0: return "transport";
        case 1: return "identity";
    }
    return "unknown";
}

bool ensureLittleFSDirLocked(const String& dir) {
    if (dir.length() == 0 || dir == "/") return true;
    if (LittleFS.exists(dir.c_str())) return true;
    int lastSlash = dir.lastIndexOf('/');
    if (lastSlash > 0) {
        if (!ensureLittleFSDirLocked(dir.substring(0, lastSlash))) return false;
    }
    return LittleFS.mkdir(dir.c_str()) || LittleFS.exists(dir.c_str());
}

class LittleFSStreamImpl : public RNS::FileStreamImpl {
public:
    explicit LittleFSStreamImpl(File&& f) : _f(std::move(f)) {}
    ~LittleFSStreamImpl() override { close(); }

protected:
    const char* name() override {
        FSLock lock;
        return _f ? _f.name() : "";
    }

    size_t size() override {
        FSLock lock;
        return _f ? _f.size() : 0;
    }

    void close() override {
        FSLock lock;
        if (_f) _f.close();
    }

    size_t write(uint8_t byte) override {
        FSLock lock;
        return _f ? _f.write(byte) : 0;
    }

    size_t write(const uint8_t* buffer, size_t len) override {
        FSLock lock;
        return _f ? _f.write(buffer, len) : 0;
    }

    int available() override {
        FSLock lock;
        return _f ? _f.available() : 0;
    }

    int read() override {
        FSLock lock;
        return _f ? _f.read() : -1;
    }

    int peek() override {
        FSLock lock;
        return _f ? _f.peek() : -1;
    }

    void flush() override {
        FSLock lock;
        if (_f) _f.flush();
    }

private:
    File _f;
};
}  // namespace

RNS::FileStream LittleFSFileSystem::open_file(const char* file_path, RNS::FileStream::MODE file_mode) {
    const char* mode = "r";
    if (file_mode == RNS::FileStream::MODE_WRITE) {
        mode = "w";
    } else if (file_mode == RNS::FileStream::MODE_APPEND) {
        mode = "a";
    } else if (file_mode != RNS::FileStream::MODE_READ) {
        return {RNS::Type::NONE};
    }

    FSLock lock;
    if (file_mode != RNS::FileStream::MODE_READ) {
        String path(file_path);
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash > 0) {
            ensureLittleFSDirLocked(path.substring(0, lastSlash));
        }
    }

    File f = LittleFS.open(file_path, mode);
    if (!f) return {RNS::Type::NONE};
    return RNS::FileStream(new LittleFSStreamImpl(std::move(f)));
}

bool LittleFSFileSystem::remove_file(const char* file_path) {
    FSLock lock;
    return LittleFS.remove(file_path);
}

bool LittleFSFileSystem::rename_file(const char* from, const char* to) {
    FSLock lock;
    return LittleFS.rename(from, to);
}

bool LittleFSFileSystem::directory_exists(const char* directory_path) {
    FSLock lock;
    return LittleFS.exists(directory_path);
}

bool LittleFSFileSystem::create_directory(const char* directory_path) {
    FSLock lock;
    return LittleFS.mkdir(directory_path);
}

bool LittleFSFileSystem::remove_directory(const char* directory_path) {
    FSLock lock;
    return LittleFS.rmdir(directory_path);
}

std::list<std::string> LittleFSFileSystem::list_directory(const char* directory_path, Callbacks::DirectoryListing callback) {
    FSLock lock;
    std::list<std::string> entries;
    File dir = LittleFS.open(directory_path);
    if (!dir || !dir.isDirectory()) return entries;
    File f = dir.openNextFile();
    while (f) {
        const char* name = f.name();
        entries.push_back(name);
        if (callback) callback(name);
        f.close();
        f = dir.openNextFile();
    }
    dir.close();
    return entries;
}

size_t LittleFSFileSystem::storage_size() {
    FSLock lock;
    return LittleFS.totalBytes();
}

size_t LittleFSFileSystem::storage_available() {
    FSLock lock;
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

// =============================================================================
// ReticulumManager
// =============================================================================

bool ReticulumManager::begin(SX1262* radio, FlashStore* flash) {
    _flash = flash;

    // Register filesystem with microReticulum (required — library throws if missing)
    LittleFSFileSystem* fsImpl = new LittleFSFileSystem();
    RNS::FileSystem fs(fsImpl);
    fs.init();
    RNS::Utilities::OS::register_filesystem(fs);
    Serial.println("[RNS] Filesystem registered");

    // Endpoint node: routing tables are rebuilt from announces on each boot.
    // No need to restore transport tables from SD — saves ~12KB heap + boot time.

    // Create and register LoRa interface. This is a mobile endpoint, so LoRa
    // uses roaming mode while Reticulum transport remains disabled below.
    _loraImpl = new LoRaInterface(radio, "LoRa.915");
    _loraIface = _loraImpl;
    _loraIface.mode(RNS::Type::Interface::MODE_ROAMING);
    RNS::Transport::register_interface(_loraIface);
    if (!_loraImpl->start()) {
        Serial.println("[RNS] WARNING: LoRa interface failed to start — radio offline");
    }
    Serial.println("[RNS] LoRa interface registered");

    // Create Reticulum instance (endpoint only — no transport/rebroadcast)
    _reticulum = RNS::Reticulum();
    // Suppress verbose microReticulum logging — LOG_TRACE floods serial at 115200 baud,
    // blocking the CPU for hundreds of ms. Change to LOG_TRACE or LOG_DEBUG for protocol debugging.
    RNS::loglevel(RNS::LOG_WARNING);
    RNS::Reticulum::transport_enabled(false);
    RNS::Reticulum::probe_destination_enabled(true);
    // Endpoint tables — 8-bit canvas frees ~32KB for these
    RNS::Transport::path_table_maxsize(48);
    RNS::Transport::announce_table_maxsize(48);
    _reticulum.start();
    Serial.println("[RNS] Reticulum started (Endpoint)");

    // Layer 1: Transport-level announce filter — runs BEFORE Ed25519 verify
    RNS::Transport::set_filter_packet_callback([](const RNS::Packet& packet) -> bool {
        if (packet.packet_type() != RNS::Type::Packet::ANNOUNCE) return true;

        unsigned long now = millis();

        // Rate limit window (per-second)
        static unsigned long windowStart = 0;
        static unsigned int count = 0;
        if (now - windowStart >= 1000) { windowStart = now; count = 0; }

        // Adaptive rate: tight during boot flood, then normal, emergency if low heap
        unsigned int maxRate;
        if (ESP.getFreeHeap() < 15000) {
            maxRate = 1;  // Emergency: almost no processing
        } else if (now < 60000) {
            maxRate = 2;  // Boot flood
        } else {
            maxRate = RSCARDPUTER_MAX_ANNOUNCES_PER_SEC;
        }
        if (++count > maxRate) return false;

        // Skip re-validation of known paths (saves ~100ms Ed25519 per announce)
        // Allow through once per 5 min for name/ratchet updates.
        // Uses raw hash bytes as key (avoids expensive toHex + hops_to per packet).
        if (RNS::Transport::has_path(packet.destination_hash())) {
            static std::unordered_map<std::string, unsigned long> lastRevalidate;
            std::string key((const char*)packet.destination_hash().data(),
                            packet.destination_hash().size());

            auto it = lastRevalidate.find(key);
            if (it != lastRevalidate.end() && (now - it->second) < 300000) return false;
            lastRevalidate[key] = now;

            // Cap map to save heap
            if (lastRevalidate.size() > 40) lastRevalidate.clear();
        }

        return true;
    });

    // Load persisted known destinations so Identity::recall() works
    // immediately after reboot for previously-seen nodes.
    RNS::Identity::load_known_destinations();

    // Load or create identity
    if (!loadOrCreateIdentity()) {
        Serial.println("[RNS] ERROR: Identity creation failed!");
        return false;
    }

    // Create LXMF delivery destination
    _destination = RNS::Destination(
        _identity,
        RNS::Type::Destination::IN,
        RNS::Type::Destination::SINGLE,
        "lxmf",
        "delivery"
    );
    _destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
    _destination.accepts_links(true);
    Serial.printf("[RNS] Destination: %s\n", _destination.hash().toHex().c_str());

    _transportActive = true;
    startPersistTask();
    Serial.println("[RNS] Endpoint active");
    return true;
}

bool ReticulumManager::loadOrCreateIdentity() {
    // Tier 1: Flash (LittleFS)
    if (_flash->exists(PATH_IDENTITY)) {
        uint8_t keyBuf[128];
        size_t keyLen = 0;
        if (_flash->readFile(PATH_IDENTITY, keyBuf, sizeof(keyBuf), keyLen) && keyLen > 0) {
            RNS::Bytes keyData(keyBuf, keyLen);
            _identity = RNS::Identity(false);
            if (_identity.load_private_key(keyData)) {
                Serial.printf("[RNS] Identity loaded from flash: %s\n", _identity.hexhash().c_str());
                saveIdentityToAll(keyData);
                return true;
            }
        }
        Serial.println("[RNS] Failed to load identity from flash");
    }

    // Tier 2: NVS (ESP32 Preferences — always available, higher trust than SD)
    {
        Preferences prefs;
        if (prefs.begin("ratcom_id", true)) {
            size_t keyLen = prefs.getBytesLength("privkey");
            if (keyLen > 0 && keyLen <= 128) {
                uint8_t keyBuf[128];
                prefs.getBytes("privkey", keyBuf, keyLen);
                prefs.end();
                RNS::Bytes keyData(keyBuf, keyLen);
                _identity = RNS::Identity(false);
                if (_identity.load_private_key(keyData)) {
                    Serial.printf("[RNS] Identity restored from NVS: %s\n", _identity.hexhash().c_str());
                    saveIdentityToAll(keyData);
                    return true;
                }
            } else {
                prefs.end();
            }
        }
    }

    // Tier 3: SD card (lowest trust — may contain another device's identity)
    if (_sd && _sd->isReady() && _sd->exists(SD_PATH_IDENTITY)) {
        uint8_t keyBuf[128];
        size_t keyLen = 0;
        if (_sd->readFile(SD_PATH_IDENTITY, keyBuf, sizeof(keyBuf), keyLen) && keyLen > 0) {
            RNS::Bytes keyData(keyBuf, keyLen);
            _identity = RNS::Identity(false);
            if (_identity.load_private_key(keyData)) {
                Serial.printf("[RNS] Identity restored from SD: %s\n", _identity.hexhash().c_str());
                saveIdentityToAll(keyData);
                return true;
            }
        }
        Serial.println("[RNS] SD identity exists but failed to load");
    }

    // No identity found anywhere — create new
    _identity = RNS::Identity();
    Serial.printf("[RNS] New identity created: %s\n", _identity.hexhash().c_str());

    RNS::Bytes privKey = _identity.get_private_key();
    if (privKey.size() > 0) {
        saveIdentityToAll(privKey);
    }
    return true;
}

void ReticulumManager::saveIdentityToAll(const RNS::Bytes& keyData) {
    // Flash
    _flash->writeAtomic(PATH_IDENTITY, keyData.data(), keyData.size());
    // SD
    if (_sd && _sd->isReady()) {
        _sd->ensureDir("/ratcom/identity");
        _sd->writeAtomic(SD_PATH_IDENTITY, keyData.data(), keyData.size());
    }
    // NVS (always available)
    Preferences prefs;
    if (prefs.begin("ratcom_id", false)) {
        prefs.putBytes("privkey", keyData.data(), keyData.size());
        prefs.end();
        Serial.println("[RNS] Identity saved to NVS");
    }
}

void ReticulumManager::loop() {
    if (!_transportActive) return;

    _reticulum.loop();
    if (_loraImpl) {
        _loraImpl->loop();
    }

    unsigned long now = millis();
    if (now - _lastPersist >= PATH_PERSIST_INTERVAL_MS) {
        _lastPersist = now;
        persistData();
    }
}

// --- Background persist task (runs on core 0) ---
// Flash writes take 0.5-4+ seconds on LittleFS. Running them on core 0
// keeps the main loop (core 1) responsive for UI and radio.

void ReticulumManager::startPersistTask() {
    _persistQueue = xQueueCreate(1, sizeof(uint8_t));
    // Pin to core 1 (same as main loop) — core 0 is WiFi/lwIP, sharing LittleFS across cores corrupts FS
    xTaskCreatePinnedToCore(persistTaskFunc, "persist", 8192, this, 1, &_persistTask, 1);
    Serial.println("[RNS] Persist task started on core 1");
}

void ReticulumManager::persistTaskFunc(void* param) {
    ReticulumManager* self = (ReticulumManager*)param;
    uint8_t cycle;
    for (;;) {
        if (xQueueReceive(self->_persistQueue, &cycle, portMAX_DELAY) == pdTRUE) {
            unsigned long start = millis();
            bool ok = true;
            switch (cycle) {
                case 0:
                    RNS::Transport::persist_data();
                    break;
                case 1:
                    RNS::Identity::persist_data();
                    break;
                // Cycle 2 removed — endpoint doesn't need SD transport backup
                default:
                    ok = false;
                    break;
            }
            Serial.printf("[PERSIST] Cycle %d done (%lums, core %d)\n",
                          cycle, millis() - start, xPortGetCoreID());
            PerfTrace::log("rns", "persist_task", persistCycleName(cycle), persistCycleName(cycle),
                           0, self->_persistQueue ? (int)uxQueueMessagesWaiting(self->_persistQueue) : -1,
                           ok, millis() - start);
        }
    }
}

void ReticulumManager::persistData() {
    unsigned long traceStart = PerfTrace::nowMs();
    uint8_t cycle = _persistCycle;
    bool ok = true;
    if (_persistQueue) {
        // Queue the cycle for background execution — non-blocking
        ok = (xQueueOverwrite(_persistQueue, &cycle) == pdPASS);
    } else {
        // Fallback: synchronous (before task is started)
        switch (cycle) {
            case 0: RNS::Transport::persist_data(); break;
            case 1: RNS::Identity::persist_data(); break;
            default: ok = false; break;
        }
    }
    PerfTrace::log("rns", _persistQueue ? "persist_queue" : "persist_sync",
                   persistCycleName(cycle), persistCycleName(cycle), 0,
                   _persistQueue ? (int)uxQueueMessagesWaiting(_persistQueue) : -1,
                   ok, PerfTrace::elapsedMs(traceStart));
    _persistCycle = (_persistCycle + 1) % 2;  // Only 2 cycles now (transport + identity)
}

String ReticulumManager::identityHash() const {
    if (!_identity) return "unknown";
    std::string hex = _identity.hexhash();
    if (hex.length() >= 12) {
        return String((hex.substr(0, 4) + ":" + hex.substr(4, 4) + ":" + hex.substr(8, 4)).c_str());
    }
    return String(hex.c_str());
}

String ReticulumManager::destinationHashStr() const {
    if (!_destination) return "unknown";
    std::string hex = _destination.hash().toHex();
    if (hex.length() >= 12) {
        return String((hex.substr(0, 6) + "::" + hex.substr(hex.length() - 6, 6)).c_str());
    }
    return String(hex.c_str());
}

String ReticulumManager::destinationHashHex() const {
    if (!_destination) return "unknown";
    return String(_destination.hash().toHex().c_str());
}

size_t ReticulumManager::pathCount() const {
    return _reticulum.get_path_table().size();
}

size_t ReticulumManager::linkCount() const {
    return _reticulum.get_link_count();
}

void ReticulumManager::announce(const RNS::Bytes& appData) {
    if (!_transportActive) return;
    _destination.announce(appData);
    _lastAnnounceTime = millis();
    Serial.println("[RNS] Announce sent");
}
