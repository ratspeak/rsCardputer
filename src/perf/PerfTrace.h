#pragma once

#include <Arduino.h>
#include <cstring>

#ifndef RSCARDPUTER_PERF_TRACE
#define RSCARDPUTER_PERF_TRACE 1
#endif

#ifndef RSCARDPUTER_PERF_WRITE_TRACE_MS
#define RSCARDPUTER_PERF_WRITE_TRACE_MS 20UL
#endif

#ifndef RSCARDPUTER_PERF_MSG_TRACE_MS
#define RSCARDPUTER_PERF_MSG_TRACE_MS 25UL
#endif

#ifndef RSCARDPUTER_PERF_UI_TRACE_MS
#define RSCARDPUTER_PERF_UI_TRACE_MS 16UL
#endif

#ifndef RSCARDPUTER_PERF_PERSIST_TRACE_MS
#define RSCARDPUTER_PERF_PERSIST_TRACE_MS 25UL
#endif

namespace PerfTrace {

inline unsigned long nowMs() {
    return millis();
}

inline unsigned long elapsedMs(unsigned long startMs) {
    return millis() - startMs;
}

inline const char* safe(const char* value) {
    return (value && value[0] != '\0') ? value : "-";
}

inline bool shouldLog(unsigned long durationMs, unsigned long thresholdMs) {
#if RSCARDPUTER_PERF_TRACE
    return durationMs >= thresholdMs;
#else
    (void)durationMs;
    (void)thresholdMs;
    return false;
#endif
}

inline unsigned long thresholdFor(const char* scope) {
    if (scope && std::strcmp(scope, "ui") == 0) return RSCARDPUTER_PERF_UI_TRACE_MS;
    if (scope && std::strcmp(scope, "msgstore") == 0) return RSCARDPUTER_PERF_MSG_TRACE_MS;
    if (scope && std::strcmp(scope, "rns") == 0) return RSCARDPUTER_PERF_PERSIST_TRACE_MS;
    if (scope && std::strcmp(scope, "announce") == 0) return RSCARDPUTER_PERF_PERSIST_TRACE_MS;
    return RSCARDPUTER_PERF_WRITE_TRACE_MS;
}

inline void log(const char* scope,
                const char* event,
                const char* backend,
                const char* path,
                size_t bytes,
                int pending,
                bool ok,
                unsigned long durationMs) {
#if RSCARDPUTER_PERF_TRACE
    if (ok && !shouldLog(durationMs, thresholdFor(scope))) return;

    Serial.printf("[PERF] scope=%s event=%s ok=%d ms=%lu bytes=%lu pending=%d backend=%s heap=%lu min=%lu path=%s\n",
                  safe(scope),
                  safe(event),
                  ok ? 1 : 0,
                  durationMs,
                  (unsigned long)bytes,
                  pending,
                  safe(backend),
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap(),
                  safe(path));
#else
    (void)scope;
    (void)event;
    (void)backend;
    (void)path;
    (void)bytes;
    (void)pending;
    (void)ok;
    (void)durationMs;
#endif
}

}  // namespace PerfTrace
