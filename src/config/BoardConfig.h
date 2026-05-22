#pragma once

// =============================================================================
// RatCom — M5Stack Cardputer Adv + Cap LoRa-1262 Pin Definitions
// =============================================================================

// --- SX1262 LoRa Radio (external SPI bus — FSPI/SPI2) ---
#define LORA_SCK    40
#define LORA_MISO   39
#define LORA_MOSI   14
#define LORA_CS      5
#define LORA_IRQ     4   // DIO1, falling edge interrupt
#define LORA_RST     3   // Active low, 100us reset pulse
#define LORA_BUSY    6   // Poll before SPI transactions
#define LORA_RXEN   -1   // Not connected
#define LORA_TXEN   -1   // Not connected

// --- SX1262 Radio Configuration ---
#define LORA_HAS_TCXO           true
#define LORA_DIO2_AS_RF_SWITCH  true
#define LORA_TCXO_VOLTAGE       0x06   // MODE_TCXO_3_0V_6X — proven on Cap LoRa-1262
#define LORA_USE_DCDC_REGULATOR false  // M5 Cap LoRa-1262 examples use LDO-only regulator mode
#define LORA_OCP_TUNED          0x28   // RNode Cardputer Adv board tuning
#define LORA_DEFAULT_FREQ       915000000
#define LORA_DEFAULT_BW         250000   // Long Fast preset (matches Ratdeck)
#define LORA_DEFAULT_SF         11
#define LORA_DEFAULT_CR         5
#define LORA_MAX_TX_POWER       22       // Cap LoRa-1262 documented maximum
#define LORA_DEFAULT_TX_POWER   22       // Long Fast preset
#define LORA_DEFAULT_PREAMBLE   18

// Cap LoRa-1262 RF antenna switch enable (PI4IOE5V6408 on Cardputer I2C).
// P0 must be driven HIGH before radio TX/RX for the external antenna path.
#define LORA_CAP_IOE_ADDR       0x43
#define LORA_CAP_RF_SW_PIN      0

// --- Keyboard (TCA8418 via I2C) ---
#define KB_SDA       8
#define KB_SCL       9
#define KB_INT      11   // Active-low, falling edge

// --- Display (ST7789V2 via M5Unified) ---
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  135

// --- GNSS (UART — reserved for v1.1) ---
#define GPS_RX      15   // GPS TX -> ESP RX
#define GPS_TX      13   // GPS RX <- ESP TX
#define GPS_BAUD    115200

// --- Audio ---
// ES8311 codec + NS4150B amp, managed by M5Unified

// --- Battery ---
// ADC via M5Unified, 1750mAh, TP4057 charger

// --- SD Card (shares FSPI/SPI2 with LoRa) ---
#define SD_CS       12   // Separate CS from LoRa (CS=5)

// --- Hardware Constants ---
#define MAX_PACKET_SIZE  255
#define SPI_FREQUENCY    8000000   // 8 MHz SPI clock for SX1262
