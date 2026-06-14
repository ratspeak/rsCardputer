#pragma once

// =============================================================================
// rsCardputer Standalone — Compile-Time Configuration
// =============================================================================

#define RSCARDPUTER_VERSION_MAJOR  2
#define RSCARDPUTER_VERSION_MINOR  0
#define RSCARDPUTER_VERSION_PATCH  1
#define RSCARDPUTER_VERSION_STRING "2.0.1"

// --- Feature Flags ---
#define HAS_DISPLAY     true
#define HAS_KEYBOARD    true
#define HAS_LORA        true
#define HAS_WIFI        true
#define HAS_BLE         true
#define HAS_SD          true
#define HAS_AUDIO       true
#define HAS_GPS         true

// --- WiFi Defaults ---
#define WIFI_AP_PORT        4242
#define WIFI_AP_PASSWORD    "ratspeak"

// --- Storage Paths ---
#define PATH_IDENTITY       "/identity/identity.key"
#define PATH_IDENTITY_BAK   "/identity/identity.key.bak"
#define PATH_PATHS          "/transport/paths.msgpack"
#define PATH_USER_CONFIG    "/config/user.json"
// Directory paths intentionally have NO trailing slash — some FATFS/VFS
// readdir paths fail to enumerate when given a path ending in '/'.
// Concat sites must add their own '/' before the basename.
#define PATH_CONTACTS       "/contacts"
#define PATH_MESSAGES       "/messages"

// --- SD Card Paths ---
// Legacy path kept intentionally so existing standalone users keep their data.
// TODO: Migrate to /rscardputer only with an explicit data migration plan.
#define SD_PATH_CONFIG_DIR   "/ratcom/config"
#define SD_PATH_USER_CONFIG  "/ratcom/config/user.json"
#define SD_PATH_MESSAGES     "/ratcom/messages"
#define SD_PATH_CONTACTS     "/ratcom/contacts"
#define SD_PATH_IDENTITY     "/ratcom/identity/identity.key"

// --- TCP Client ---
#define MAX_TCP_CONNECTIONS         4
#define TCP_DEFAULT_PORT            4242
#define TCP_RECONNECT_INTERVAL_MS   10000
#define TCP_CONNECT_TIMEOUT_MS      5000

// --- Announce Flood Defense ---
#define RSCARDPUTER_MAX_ANNOUNCES_PER_SEC 4     // Transport-level rate limit (before Ed25519 verify)

// --- Limits ---
#define RSCARDPUTER_MAX_NODES           50
// Global message limits (total across ALL conversations)
#define RSCARDPUTER_MSG_LIMIT_SD         5000  // With SD card — generous
#define RSCARDPUTER_MSG_LIMIT_FLASH       200  // Flash only — tight (1.88MB partition)
#define RSCARDPUTER_MSG_WARN_PCT           90  // Warn user at 90% capacity
#define FLASH_MSG_CACHE_LIMIT         20  // Flash cache per conv when SD is primary
#define RSCARDPUTER_MAX_OUTQUEUE        20   // Cap LXMF outgoing queue
#define PATH_PERSIST_INTERVAL_MS 300000  // 5 min — endpoint rebuilds from announces

// --- Power Management ---
#define SCREEN_DIM_TIMEOUT_MS   30000
#define SCREEN_OFF_TIMEOUT_MS   60000
#define SCREEN_DIM_BRIGHTNESS   64     // 25% of 255

// --- Radio Regions ---
enum RadioRegion : uint8_t {
    REGION_AMERICAS  = 0,  // 915 MHz (902-928 ISM)
    REGION_EUROPE    = 1,  // 868 MHz (863-870)
    REGION_AUSTRALIA = 2,  // 915 MHz (915-928)
    REGION_ASIA      = 3,  // 923 MHz (AS923)
    REGION_COUNT     = 4
};

static constexpr uint32_t REGION_FREQ[REGION_COUNT] = {
    915000000, 868000000, 915000000, 923000000
};

static const char* const REGION_LABELS[REGION_COUNT] = {
    "Americas (915)", "Europe (868)", "Australia (915)", "Asia (923)"
};

// --- Serial Debug ---
#define SERIAL_BAUD  115200
