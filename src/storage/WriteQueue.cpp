#include "WriteQueue.h"
#include "storage/SDStore.h"
#include "storage/FlashStore.h"
#include "perf/PerfTrace.h"
#include <Preferences.h>

namespace {
const char* backendName(WriteBackend backend) {
    switch (backend) {
        case WriteBackend::SD_ONLY: return "sd";
        case WriteBackend::FLASH_ONLY: return "flash";
        case WriteBackend::BOTH: return "both";
    }
    return "unknown";
}

const char* primaryPath(const char* sdPath, const char* flashPath) {
    if (sdPath && sdPath[0] != '\0') return sdPath;
    return flashPath;
}
}  // namespace

bool WriteQueue::begin(SDStore* sd, FlashStore* flash) {
    _sd = sd;
    _flash = flash;

    _queue = xQueueCreate(QUEUE_DEPTH, sizeof(WriteJob*));
    if (!_queue) {
        Serial.println("[WRITEQ] Failed to create queue");
        return false;
    }

    // Pin to Core 1 (same as main loop) — LittleFS is NOT thread-safe across cores
    xTaskCreatePinnedToCore(taskFunc, "WriteQ", TASK_STACK, this, TASK_PRIORITY, &_task, 1);
    if (!_task) {
        Serial.println("[WRITEQ] Failed to create task");
        return false;
    }

    Serial.println("[WRITEQ] Async write queue started on Core 1");
    return true;
}

bool WriteQueue::enqueue(const char* sdPath, const char* flashPath, const String& data, WriteBackend backend) {
    unsigned long traceStart = PerfTrace::nowMs();
    const size_t bytes = data.length();
    const char* tracePath = primaryPath(sdPath, flashPath);

    if (!_queue) {
        PerfTrace::log("writeq", "enqueue", backendName(backend), tracePath, bytes,
                       _pending, false, PerfTrace::elapsedMs(traceStart));
        return false;
    }

    WriteJob* job = new (std::nothrow) WriteJob();
    if (!job) {
        PerfTrace::log("writeq", "enqueue", backendName(backend), tracePath, bytes,
                       _pending, false, PerfTrace::elapsedMs(traceStart));
        return false;
    }

    job->sdPath[0] = '\0';
    job->flashPath[0] = '\0';
    if (sdPath) {
        strncpy(job->sdPath, sdPath, sizeof(job->sdPath) - 1);
        job->sdPath[sizeof(job->sdPath) - 1] = '\0';
    }
    if (flashPath) {
        strncpy(job->flashPath, flashPath, sizeof(job->flashPath) - 1);
        job->flashPath[sizeof(job->flashPath) - 1] = '\0';
    }
    job->data = data;
    job->backend = backend;

    if (xQueueSend(_queue, &job, 0) != pdTRUE) {
        delete job;
        Serial.println("[WRITEQ] Queue full, dropping write");
        PerfTrace::log("writeq", "enqueue", backendName(backend), tracePath, bytes,
                       _pending, false, PerfTrace::elapsedMs(traceStart));
        return false;
    }

    _pending++;
    PerfTrace::log("writeq", "enqueue", backendName(backend), tracePath, bytes,
                   _pending, true, PerfTrace::elapsedMs(traceStart));
    return true;
}

bool WriteQueue::enqueue(const char* path, const String& data, WriteBackend backend) {
    if (backend == WriteBackend::SD_ONLY) {
        return enqueue(path, nullptr, data, backend);
    } else if (backend == WriteBackend::FLASH_ONLY) {
        return enqueue(nullptr, path, data, backend);
    }
    // BOTH requires dual-path overload
    return enqueue(path, path, data, backend);
}

void WriteQueue::waitForFlush(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (_pending > 0 && (millis() - start) < timeoutMs) {
        delay(5);  // Yield to let WriteQueue task process
    }
    if (_pending > 0) {
        Serial.printf("[WRITEQ] Flush timeout (%d pending)\n", (int)_pending);
    }
}

bool WriteQueue::isFull() const {
    if (!_queue) return true;
    return uxQueueSpacesAvailable(_queue) == 0;
}

void WriteQueue::taskFunc(void* param) {
    WriteQueue* self = static_cast<WriteQueue*>(param);
    WriteJob* job = nullptr;

    for (;;) {
        if (xQueueReceive(self->_queue, &job, pdMS_TO_TICKS(1000)) == pdTRUE) {
            self->processJob(*job);
            delete job;
            if (self->_pending > 0) self->_pending--;
        }

        // Periodic maintenance (NVS counter persist)
        if (millis() - self->_lastMaintenance >= MAINTENANCE_INTERVAL) {
            self->periodicMaintenance();
            self->_lastMaintenance = millis();
        }
    }
}

void WriteQueue::processJob(const WriteJob& job) {
    unsigned long jobStart = PerfTrace::nowMs();
    const size_t bytes = job.data.length();
    bool ok = true;

    // SD write
    if (job.backend == WriteBackend::SD_ONLY || job.backend == WriteBackend::BOTH) {
        unsigned long backendStart = PerfTrace::nowMs();
        bool backendOk = false;
        if (_sd && _sd->isReady() && job.sdPath[0] != '\0') {

            // Ensure parent directory exists on SD
            String sdDir = String(job.sdPath);
            int lastSlash = sdDir.lastIndexOf('/');
            if (lastSlash > 0) {
                sdDir = sdDir.substring(0, lastSlash);
                _sd->ensureDir(sdDir.c_str());
            }

            backendOk = _sd->writeDirect(job.sdPath, (const uint8_t*)job.data.c_str(), job.data.length());
        }
        if (!backendOk) {
            Serial.printf("[WRITEQ] SD write FAILED: %s\n", job.sdPath);
            ok = false;
        }
        PerfTrace::log("writeq", "process", "sd", job.sdPath, bytes, _pending, backendOk,
                       PerfTrace::elapsedMs(backendStart));
    }

    // Flash write
    if (job.backend == WriteBackend::FLASH_ONLY || job.backend == WriteBackend::BOTH) {
        unsigned long backendStart = PerfTrace::nowMs();
        bool backendOk = false;
        if (_flash && _flash->isReady() && job.flashPath[0] != '\0') {

            // Ensure parent directory exists on flash
            String flashDir = String(job.flashPath);
            int lastSlash = flashDir.lastIndexOf('/');
            if (lastSlash > 0) {
                flashDir = flashDir.substring(0, lastSlash);
                _flash->ensureDir(flashDir.c_str());
            }

            backendOk = _flash->writeDirect(job.flashPath, (const uint8_t*)job.data.c_str(), job.data.length());
        }
        if (!backendOk) {
            Serial.printf("[WRITEQ] Flash write FAILED: %s\n", job.flashPath);
            ok = false;
        }
        PerfTrace::log("writeq", "process", "flash", job.flashPath, bytes, _pending, backendOk,
                       PerfTrace::elapsedMs(backendStart));
    }

    PerfTrace::log("writeq", "process_job", backendName(job.backend),
                   primaryPath(job.sdPath, job.flashPath), bytes, _pending, ok,
                   PerfTrace::elapsedMs(jobStart));
}

void WriteQueue::periodicMaintenance() {
    unsigned long traceStart = PerfTrace::nowMs();
    bool ok = true;
    size_t bytes = 0;

    // Persist receive counter to NVS (batched, not per-message)
    if (_counterRef) {
        uint32_t current = _counterRef->load(std::memory_order_relaxed);
        if (current != _lastPersistedCounter) {
            Preferences prefs;
            if (prefs.begin("ratcom", false)) {
                prefs.putUInt("msgctr", current);
                prefs.end();
                _lastPersistedCounter = current;
                bytes = sizeof(current);
            } else {
                ok = false;
            }
        }
    }

    PerfTrace::log("writeq", "maintenance", "nvs", "ratcom/msgctr", bytes,
                   _pending, ok, PerfTrace::elapsedMs(traceStart));
}
