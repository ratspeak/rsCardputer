#include "SDStore.h"
#include "config/Config.h"
#include "hal/SharedSPIBus.h"

bool SDStore::begin(SPIClass* spi, int csPin) {
    if (!spi) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;

    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);
    delay(10);

    if (!SD.begin(csPin, *spi, 4000000)) {
        Serial.printf("[SD] Mount failed (CS=%d), retrying...\n", csPin);
        delay(100);
        if (!SD.begin(csPin, *spi, 4000000)) {
            Serial.println("[SD] Card not detected or mount failed");
            _ready = false;
            return false;
        }
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No card inserted");
        _ready = false;
        return false;
    }

    const char* typeStr = "UNKNOWN";
    if (cardType == CARD_MMC)  typeStr = "MMC";
    if (cardType == CARD_SD)   typeStr = "SD";
    if (cardType == CARD_SDHC) typeStr = "SDHC";

    _ready = true;

    SD.end();
    if (!SD.begin(csPin, *spi, 16000000)) {
        if (!SD.begin(csPin, *spi, 4000000)) {
            Serial.println("[SD] 16MHz failed and 4MHz fallback failed");
            _ready = false;
            return false;
        }
        Serial.println("[SD] 16MHz failed, using 4MHz");
    } else {
        Serial.println("[SD] SPI speed: 16MHz");
    }
    _ready = true;

    Serial.printf("[SD] %s card ready, total=%llu MB, used=%llu MB\n",
                  typeStr, totalBytes() / (1024 * 1024), usedBytes() / (1024 * 1024));
    return true;
}

void SDStore::end() {
    SharedSPILock lock;
    if (!lock.locked()) return;
    SD.end();
    _ready = false;
}

uint64_t SDStore::totalBytes() const {
    if (!_ready) return 0;
    SharedSPILock lock;
    if (!lock.locked()) return 0;
    return SD.totalBytes();
}

uint64_t SDStore::usedBytes() const {
    if (!_ready) return 0;
    SharedSPILock lock;
    if (!lock.locked()) return 0;
    return SD.usedBytes();
}

bool SDStore::ensureDir(const char* path) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
}

bool SDStore::exists(const char* path) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    return SD.exists(path);
}

bool SDStore::remove(const char* path) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    return SD.remove(path);
}

File SDStore::openDir(const char* path) {
    if (!_ready) return File();
    SharedSPILock lock;
    if (!lock.locked()) return File();
    return SD.open(path);
}

bool SDStore::removeDir(const char* path) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    return SD.rmdir(path);
}

bool SDStore::readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead) {
    bytesRead = 0;
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t size = f.size();
    if (size > maxLen) { f.close(); return false; }
    bytesRead = f.read(buffer, size);
    f.close();
    return bytesRead == size;
}

bool SDStore::writeAtomic(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;

    String tmpPath = String(path) + ".tmp";
    String bakPath = String(path) + ".bak";

    // Step 1: Write to .tmp
    File f = SD.open(tmpPath.c_str(), FILE_WRITE);
    if (!f) {
        Serial.printf("[SD] Failed to open %s for write\n", tmpPath.c_str());
        return false;
    }
    size_t written = f.write(data, len);
    f.close();
    if (written != len) {
        SD.remove(tmpPath.c_str());
        return false;
    }

    // Step 2: Verify .tmp by reading back size
    File verify = SD.open(tmpPath.c_str(), FILE_READ);
    if (!verify || verify.size() != len) {
        if (verify) verify.close();
        SD.remove(tmpPath.c_str());
        return false;
    }
    verify.close();

    // Step 3: Rename current to .bak (if exists)
    if (SD.exists(path)) {
        SD.remove(bakPath.c_str());
        if (!SD.rename(path, bakPath.c_str())) {
            SD.remove(tmpPath.c_str());
            Serial.printf("[SD] writeAtomic: backup rename failed for %s\n", path);
            return false;
        }
    }

    SD.remove(path);

    // Step 4: Rename .tmp to primary
    if (!SD.rename(tmpPath.c_str(), path)) {
        // Restore from backup on failure
        if (SD.exists(bakPath.c_str())) {
            SD.rename(bakPath.c_str(), path);
        }
        SD.remove(tmpPath.c_str());
        return false;
    }

    SD.remove(bakPath.c_str());

    return true;
}

bool SDStore::writeString(const char* path, const String& data) {
    return writeAtomic(path, (const uint8_t*)data.c_str(), data.length());
}

bool SDStore::writeDirect(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write(data, len);
    f.flush();
    f.close();
    return written == len;
}

String SDStore::readString(const char* path) {
    if (!_ready) return "";
    SharedSPILock lock;
    if (!lock.locked()) return "";

    File f = SD.open(path, FILE_READ);
    if (!f) {
        // Try backup
        String bakPath = String(path) + ".bak";
        f = SD.open(bakPath.c_str(), FILE_READ);
        if (!f) return "";
        Serial.printf("[SD] Restored from backup: %s\n", path);
    }
    // Guard against corrupted/huge files exhausting heap
    if (f.size() > 16384) {
        Serial.printf("[SD] readString: file too large (%d bytes): %s\n", (int)f.size(), path);
        f.close();
        return "";
    }
    String result = f.readString();
    f.close();
    return result;
}

bool SDStore::wipeRatcom() {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    Serial.println("[SD] Wiping /ratcom/ ...");
    wipeDir("/ratcom/messages");
    wipeDir("/ratcom/contacts");
    wipeDir("/ratcom/identity");
    wipeDir("/ratcom/config");
    SD.rmdir("/ratcom");
    Serial.println("[SD] Wipe complete, recreating dirs...");
    return formatForRatcom();
}

void SDStore::wipeDir(const char* path) {
    SharedSPILock lock;
    if (!lock.locked()) return;
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) return;
    File entry = dir.openNextFile();
    while (entry) {
        String fullPath = String(path) + "/" + entry.name();
        if (entry.isDirectory()) {
            wipeDir(fullPath.c_str());
            SD.rmdir(fullPath.c_str());
        } else {
            SD.remove(fullPath.c_str());
        }
        entry = dir.openNextFile();
    }
    dir.close();
}

bool SDStore::hasExistingData() {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;
    if (SD.exists(SD_PATH_USER_CONFIG)) return true;
    if (SD.exists(SD_PATH_IDENTITY)) return true;
    File dir = SD.open(SD_PATH_MESSAGES);
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        bool found = (bool)entry;
        if (entry) entry.close();
        dir.close();
        if (found) return true;
    }
    return false;
}

bool SDStore::formatForRatcom() {
    if (!_ready) return false;
    SharedSPILock lock;
    if (!lock.locked()) return false;

    Serial.println("[SD] Creating RatCom directory structure...");
    bool ok = true;
    ok &= ensureDir("/ratcom");
    ok &= ensureDir("/ratcom/config");
    ok &= ensureDir("/ratcom/messages");
    ok &= ensureDir("/ratcom/contacts");
    ok &= ensureDir("/ratcom/identity");

    if (ok) {
        Serial.println("[SD] Directory structure ready");
    } else {
        Serial.println("[SD] Failed to create directories");
    }
    return ok;
}
