#include "SharedSPIBus.h"

static SemaphoreHandle_t sharedSPIMutexHandle() {
    static SemaphoreHandle_t sharedSPIMutex = xSemaphoreCreateRecursiveMutex();
    return sharedSPIMutex;
}

SharedSPILock::SharedSPILock(TickType_t timeout) {
    SemaphoreHandle_t mutex = sharedSPIMutexHandle();
    _locked = mutex && (xSemaphoreTakeRecursive(mutex, timeout) == pdTRUE);
}

SharedSPILock::~SharedSPILock() {
    if (_locked) {
        xSemaphoreGiveRecursive(sharedSPIMutexHandle());
    }
}
