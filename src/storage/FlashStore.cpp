#include "FlashStore.h"
#include "perf/PerfTrace.h"

// Global LittleFS mutex — prevents cross-task corruption
SemaphoreHandle_t FlashStore::_mutex = nullptr;

SemaphoreHandle_t FlashStore::mutex() {
    return _mutex;
}

// RAII lock guard for the mutex
struct FSLock {
    FSLock(SemaphoreHandle_t m) : _m(m) {
        if (_m) xSemaphoreTake(_m, portMAX_DELAY);
    }
    ~FSLock() {
        if (_m) xSemaphoreGive(_m);
    }
    SemaphoreHandle_t _m;
};

namespace {
const char* FLASH_PARTITION_LABELS[] = {"littlefs", "spiffs"};
constexpr const char* FLASH_BASE_PATH = "/littlefs";

const char* mountLittleFS(bool formatOnFail) {
    for (const char* label : FLASH_PARTITION_LABELS) {
        if (LittleFS.begin(formatOnFail, FLASH_BASE_PATH, 10, label)) {
            Serial.printf("[FLASH] LittleFS %s on partition '%s'\n",
                          formatOnFail ? "formatted and mounted" : "mounted",
                          label);
            return label;
        }
    }
    return nullptr;
}

bool ensureDirLocked(const char* path) {
    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) return true;
    if (LittleFS.exists(path)) return true;

    String pathStr(path);
    int lastSlash = pathStr.lastIndexOf('/');
    if (lastSlash > 0) {
        String parent = pathStr.substring(0, lastSlash);
        if (!ensureDirLocked(parent.c_str())) return false;
    }
    return LittleFS.mkdir(path) || LittleFS.exists(path);
}

String fullPathForEntry(const char* dirPath, const char* entryName) {
    if (!entryName || entryName[0] == '\0') return "";
    String name(entryName);
    if (name.startsWith("/")) return name;
    String dir(dirPath);
    if (dir.endsWith("/")) return dir + name;
    return dir + "/" + name;
}

void recoverAtomicArtifact(const String& artifactPath,
                           int& recovered,
                           int& removedTmp,
                           int& removedBak) {
    if (artifactPath.endsWith(".tmp")) {
        if (LittleFS.remove(artifactPath.c_str())) removedTmp++;
        return;
    }

    if (!artifactPath.endsWith(".bak")) return;

    String primaryPath = artifactPath.substring(0, artifactPath.length() - 4);
    if (LittleFS.exists(primaryPath.c_str())) {
        if (LittleFS.remove(artifactPath.c_str())) removedBak++;
        return;
    }

    if (LittleFS.rename(artifactPath.c_str(), primaryPath.c_str())) {
        recovered++;
        Serial.printf("[FLASH] Recovered backup: %s\n", primaryPath.c_str());
    } else {
        Serial.printf("[FLASH] Failed to recover backup: %s\n", artifactPath.c_str());
    }
}

void recoverArtifactsInDir(const char* dirPath,
                           int& recovered,
                           int& removedTmp,
                           int& removedBak) {
    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        String fullPath = fullPathForEntry(dirPath, entry.name());
        bool isArtifact = fullPath.endsWith(".tmp") || fullPath.endsWith(".bak");
        entry.close();
        if (isArtifact) {
            recoverAtomicArtifact(fullPath, recovered, removedTmp, removedBak);
        }
        entry = dir.openNextFile();
    }
    dir.close();
}
}  // namespace

bool FlashStore::begin() {
    // Create mutex before any LittleFS access
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }

    FSLock lock(_mutex);
    _ready = false;

    const char* mountedLabel = mountLittleFS(false);
    if (!mountedLabel) {
        Serial.println("[FLASH] LittleFS mount failed on known labels; formatting data partition...");
        mountedLabel = mountLittleFS(true);
        if (!mountedLabel) {
            Serial.println("[FLASH] LittleFS format/mount failed on all known labels!");
            return false;
        }
    }
    _ready = true;

    // Ensure required directories
    ensureDirLocked("/identity");
    ensureDirLocked("/transport");
    ensureDirLocked("/config");
    ensureDirLocked("/contacts");
    ensureDirLocked("/messages");

    Serial.printf("[FLASH] LittleFS ready, total=%lu, used=%lu\n",
                  (unsigned long)LittleFS.totalBytes(),
                  (unsigned long)LittleFS.usedBytes());

    recoverAtomicArtifacts();

    return true;
}

void FlashStore::recoverAtomicArtifacts() {
    static const char* dirs[] = {"/config", "/contacts", "/identity", "/transport"};
    int recovered = 0;
    int removedTmp = 0;
    int removedBak = 0;

    for (const char* dirPath : dirs) {
        recoverArtifactsInDir(dirPath, recovered, removedTmp, removedBak);
    }

    File msgRoot = LittleFS.open("/messages");
    if (msgRoot && msgRoot.isDirectory()) {
        File peerDir = msgRoot.openNextFile();
        while (peerDir) {
            if (peerDir.isDirectory()) {
                String peerPath = fullPathForEntry("/messages", peerDir.name());
                peerDir.close();
                recoverArtifactsInDir(peerPath.c_str(), recovered, removedTmp, removedBak);
            } else {
                peerDir.close();
            }
            peerDir = msgRoot.openNextFile();
        }
    }
    if (msgRoot) msgRoot.close();

    if (recovered || removedTmp || removedBak) {
        Serial.printf("[FLASH] Atomic recovery: restored=%d removed_tmp=%d removed_bak=%d\n",
                      recovered, removedTmp, removedBak);
    }
}

void FlashStore::end() {
    FSLock lock(_mutex);
    LittleFS.end();
    _ready = false;
}

bool FlashStore::ensureDir(const char* path) {
    if (!_ready) return false;
    FSLock lock(_mutex);
    return ensureDirLocked(path);
}

bool FlashStore::exists(const char* path) {
    if (!_ready) return false;
    FSLock lock(_mutex);
    return LittleFS.exists(path);
}

bool FlashStore::remove(const char* path) {
    if (!_ready) return false;
    FSLock lock(_mutex);
    return LittleFS.remove(path);
}

bool FlashStore::removeDir(const char* path) {
    if (!_ready) return false;
    FSLock lock(_mutex);
    return LittleFS.rmdir(path);
}

bool FlashStore::rename(const char* from, const char* to) {
    if (!_ready) return false;
    FSLock lock(_mutex);
    return LittleFS.rename(from, to);
}

File FlashStore::openDir(const char* path) {
    if (!_ready) return File();
    FSLock lock(_mutex);
    return LittleFS.open(path);
}

File FlashStore::openFile(const char* path, const char* mode) {
    if (!_ready) return File();
    FSLock lock(_mutex);
    return LittleFS.open(path, mode);
}

bool FlashStore::writeAtomic(const char* path, const uint8_t* data, size_t len) {
    unsigned long traceStart = PerfTrace::nowMs();
    auto finishTrace = [&](bool ok) {
        PerfTrace::log("flash", "write_atomic", "flash", path, len, -1, ok,
                       PerfTrace::elapsedMs(traceStart));
        return ok;
    };

    if (!_ready) return finishTrace(false);
    // Refuse writes when heap is critically low — prevents OOM-induced LittleFS unmount
    if (ESP.getFreeHeap() < 4096) {
        Serial.printf("[FLASH] Write refused (heap=%lu) — OOM protection: %s\n",
                      (unsigned long)ESP.getFreeHeap(), path);
        return finishTrace(false);
    }
    FSLock lock(_mutex);

    // Step 1: Write to .tmp
    String tmpPath = String(path) + ".tmp";
    String bakPath = String(path) + ".bak";

    File f = LittleFS.open(tmpPath.c_str(), "w");
    if (!f) return finishTrace(false);
    size_t written = f.write(data, len);
    f.close();
    if (written != len) {
        LittleFS.remove(tmpPath.c_str());
        return finishTrace(false);
    }

    // Step 2: Verify .tmp by reading back
    File verify = LittleFS.open(tmpPath.c_str(), "r");
    if (!verify || verify.size() != len) {
        if (verify) verify.close();
        LittleFS.remove(tmpPath.c_str());
        return finishTrace(false);
    }
    verify.close();

    // Step 3: Rename current to .bak (if exists)
    if (LittleFS.exists(path)) {
        LittleFS.remove(bakPath.c_str());
        if (!LittleFS.rename(path, bakPath.c_str())) {
            LittleFS.remove(tmpPath.c_str());
            Serial.printf("[FLASH] writeAtomic: backup rename failed for %s\n", path);
            return finishTrace(false);
        }
    }

    // Step 4: Rename .tmp to primary
    if (!LittleFS.rename(tmpPath.c_str(), path)) {
        // Restore from backup on failure
        if (LittleFS.exists(bakPath.c_str())) {
            LittleFS.rename(bakPath.c_str(), path);
        }
        LittleFS.remove(tmpPath.c_str());
        return finishTrace(false);
    }

    // Step 5: Remove .bak (no longer needed)
    LittleFS.remove(bakPath.c_str());

    return finishTrace(true);
}

bool FlashStore::readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead) {
    if (!_ready) return false;
    FSLock lock(_mutex);

    // Try primary first
    File f = LittleFS.open(path, "r");
    if (!f) {
        // Try backup
        String bakPath = String(path) + ".bak";
        f = LittleFS.open(bakPath.c_str(), "r");
        if (!f) return false;
        Serial.printf("[FLASH] Restored from backup: %s\n", path);
    }

    bytesRead = f.readBytes((char*)buffer, maxLen);
    f.close();
    return bytesRead > 0;
}

bool FlashStore::writeString(const char* path, const String& data) {
    return writeAtomic(path, (const uint8_t*)data.c_str(), data.length());
}

bool FlashStore::writeDirect(const char* path, const uint8_t* data, size_t len) {
    unsigned long traceStart = PerfTrace::nowMs();
    auto finishTrace = [&](bool ok) {
        PerfTrace::log("flash", "write_direct", "flash", path, len, -1, ok,
                       PerfTrace::elapsedMs(traceStart));
        return ok;
    };

    if (!_ready) return finishTrace(false);
    if (ESP.getFreeHeap() < 4096) {
        Serial.printf("[FLASH] Write refused (heap=%lu) — OOM protection: %s\n",
                      (unsigned long)ESP.getFreeHeap(), path);
        return finishTrace(false);
    }
    FSLock lock(_mutex);
    File f = LittleFS.open(path, "w");
    if (!f) return finishTrace(false);
    size_t written = f.write(data, len);
    f.flush();
    f.close();
    return finishTrace(written == len);
}

String FlashStore::readString(const char* path) {
    if (!_ready) return "";
    FSLock lock(_mutex);
    File f = LittleFS.open(path, "r");
    if (!f) {
        String bakPath = String(path) + ".bak";
        f = LittleFS.open(bakPath.c_str(), "r");
        if (!f) return "";
    }
    // Guard against corrupted/huge files exhausting heap
    if (f.size() > 8192) {
        Serial.printf("[FLASH] readString: file too large (%d bytes): %s\n", (int)f.size(), path);
        f.close();
        return "";
    }
    String result = f.readString();
    f.close();
    return result;
}

bool FlashStore::isReady() {
    return _ready;
}

bool FlashStore::format() {
    Serial.println("[FLASH] Formatting LittleFS...");
    {
        FSLock lock(_mutex);
        LittleFS.end();
        _ready = false;
        LittleFS.format();
    }
    // Re-mount with fresh filesystem (begin() takes its own lock)
    return begin();
}
