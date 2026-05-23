# RatCom — Troubleshooting

Collected hardware and software gotchas, organized by category.

---

## Radio Issues

### TCXO voltage must be 3.0V

The Cap LoRa-1262 uses a TCXO (temperature-compensated crystal oscillator) that requires exactly 3.0V. This is configured as `MODE_TCXO_3_0V_6X` (0x06) in `BoardConfig.h`.

**Symptom**: Radio reports online but can't synthesize frequencies. PLL lock fails, no TX or RX.

**Diagnosis**: Check `Ctrl+D` diagnostics — if `DevErrors` shows `0x0040`, that's PLL lock failure.

**Fix**: Verify `LORA_TCXO_VOLTAGE` is `0x06` in `BoardConfig.h`.

### IRQ stale latch fix

The SX1262's IRQ flags can become latched from previous operations. If stale flags persist, DCD (detect channel activity) gets stuck in "channel busy" state, CSMA blocks, and TX never fires.

**Symptom**: First packet sends fine, then all subsequent TX attempts hang. DCD reports channel busy even with nothing transmitting.

**Fix** (applied in `SX1262.cpp`):
- Clear all IRQ flags at the top of `receive()` before entering RX mode
- In `dcd()`, clear stale header error flags when preamble is not detected

### `_txp` is a base class member

The `_txp` (TX power) field is declared in `RadioInterface` base class (inherited from RNode firmware lineage). It cannot be initialized in the `sx126x` constructor initializer list — must set `_txp = 14` in the constructor body.

**Symptom**: TX power reads as 0, which may cause silent failures or very weak transmission.

### PA ramp time

Use 40μs PA ramp time for the Cap LoRa-1262. This is set during `setTxParams()` in the SX1262 driver. Faster ramp times may cause spectral splatter; slower wastes time.

---

## Build Issues

### PlatformIO not on PATH

After `pip install platformio`, the `pio` binary may not be in your shell's PATH.

**Fix**: Use `python3 -m platformio` instead of `pio`:

```bash
python3 -m platformio run -e ratcom_915
python3 -m platformio run -e ratcom_915 -t upload
python3 -m platformio device monitor -b 115200
```

The recommended Cardputer Adv family build uses the top-level Makefile:

```bash
make package
make flash port=/dev/cu.usbmodem3101
```

### esptool baud rate

PlatformIO defaults to 921600 baud for flashing, which sometimes fails with USB-Serial/JTAG.

**Fix**: Use 460800 baud with esptool directly:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* --baud 460800 \
    write-flash -z 0x10000 .pio/build/ratcom_915/firmware.bin
```

### esptool hyphenated flags

esptool deprecated underscored command names. Use hyphens:

| Correct | Deprecated |
|---------|-----------|
| `merge-bin` | `merge_bin` |
| `write-flash` | `write_flash` |
| `--flash-mode` | `--flash_mode` |
| `--flash-size` | `--flash_size` |

### USBMode must be `default`

The build flag `ARDUINO_USB_MODE=1` selects USB-Serial/JTAG mode (not native CDC).

**Symptom**: With `hwcdc` (USB_MODE=0), the native USB CDC peripheral doesn't enumerate on macOS in firmware mode. The port never appears.

**Fix**: Keep `ARDUINO_USB_MODE=1` in `platformio.ini`. The port appears as `/dev/cu.usbmodem<chipID>` (not `usbmodem5101`, which is bootloader-only).

---

## Boot Issues

### Boot loop detection and recovery

RatCom tracks consecutive boot failures in NVS (non-volatile storage, separate from LittleFS). If 3 consecutive boots fail to reach the end of `setup()`, WiFi is forced OFF on the next boot.

**How it works**:
1. On each boot, NVS counter `bootc` increments
2. If `bootc >= 3`, `bootLoopRecovery = true` → WiFi forced to `RAT_WIFI_OFF`
3. At the end of successful `setup()`, counter resets to 0

**Manual recovery**: If the device is boot-looping, connect serial at 115200 baud and watch for the `[BOOT] Boot loop detected` message. The device should stabilize with WiFi off, then you can change WiFi settings.

### Root cause: Transport reference stability

The original boot loop was caused by `RNS::Transport::_interfaces` storing `Interface&` (references, NOT copies). A local `RNS::Interface iface(tcp)` declared inside a loop or function would go out of scope, creating a dangling reference. When Reticulum's transport loop tried to access it: `LoadProhibited` crash.

**Fix**: Store TCP Interface wrappers in `std::list<RNS::Interface> tcpIfaces` at global scope. Must use `std::list`, not `std::vector` — vector reallocation would move objects in memory, invalidating references held by Transport.

### "auto detect board:24" in serial output

This is the M5Unified library auto-detecting the board type. It's informational, not an error. The number 24 is M5's internal board ID for Cardputer Adv.

---

## Storage Issues

### LittleFS not mounting

**Symptom**: `[E][vfs_api.cpp:24] open(): File system is not mounted` during boot.

**Possible causes**:
- First boot after flash erase — LittleFS partition needs formatting
- Partition table mismatch — verify the flashed image matches either the dual-mode table in `partitions/` or the standalone table `partitions_8MB_ota.csv`
- `flash.begin()` may fail silently

**Workaround**: FlashStore attempts `LittleFS.begin(true)` which auto-formats on first use. If it persists, erase the LittleFS partition and reflash.

### SD card directories missing

On first boot with a new SD card, the `/ratcom/` directory tree doesn't exist.

**Fix**: `setup()` calls `sdStore.ensureDir()` for all required paths after SD init. If directories are still missing, check that SD CS (GPIO 12) is not conflicting with radio SPI.

---

## Interop & RF Issues

### RatCom TX/RX verification (QA Round 9)

- **RatCom RX confirmed**: Received Heltec V3 RNode announce at -38 dBm, SNR 13.0
- **RatCom TX confirmed**: All SX1262 registers verified correct (SF9, BW 125 kHz, CR 4/5, sync 0x1424, CRC on)
- **Heltec V3 RNode receive path**: Has never decoded a single LoRa packet from any source. Shows RatCom RF as interference (-50 to -81 dBm) but can't decode. This is a Heltec issue, not RatCom.

### Debugging RF with RSSI Monitor

Press **Ctrl+R** to sample RSSI continuously for 5 seconds. Transmit from another device during sampling. If RSSI stays at the noise floor (around -110 to -120 dBm), the RX front-end isn't hearing RF.

### Radio test packet

Press **Ctrl+T** to send a test packet with a fixed header (0xA0) and payload `RATCOM_TEST_1234567890`. Includes FIFO readback verification. Use this to confirm the TX path is working without involving Reticulum.

---

## WiFi Issues

### AP and STA are separate modes

RatCom uses **three WiFi modes**: OFF, AP, STA. These are NOT concurrent — `WIFI_AP_STA` was removed because it consumed ~20KB extra heap and caused instability.

- **AP mode**: Creates `ratcom-XXXX` hotspot, runs TCP server on port 4242
- **STA mode**: Connects to an existing network, creates TCP client connections to configured endpoints
- **OFF**: No WiFi (saves power and heap)

Switch modes in Settings → WiFi.

### TCP client connection lifecycle

In STA mode, TCP client connections to configured endpoints are created **once** on first WiFi connection. If WiFi drops and reconnects, existing TCP clients attempt reconnection automatically (every 10 seconds).

---

## Known Limitations

| Feature | Status | Notes |
|---------|--------|-------|
| BLE in Ratcom mode | Disabled (stub) | RNode mode provides BLE pairing for host-controlled use |
| GNSS | Implemented | GPS RX=15, TX=13 |
| OTA updates | Not implemented | The dual-mode image uses OTA app slots locally for launcher/Ratcom/RNode selection |
| LittleFS | Occasional mount failures | Auto-formats on first use; rare failures after that |
| WiFi AP+STA | Removed | Uses too much heap; separate AP/STA modes instead |
| Split packets | Header flag defined, not implemented | Single-frame LoRa only (max 254 bytes payload) |

---

## Factory Reset

### SD card wipe (preserves flash data)

Connect serial at 115200 baud. Power cycle the device and send `WIPE` within 500ms of boot. This recursively deletes `/ratcom/*` on the SD card and recreates clean directories.

### Flash erase (full reset)

Erase all flash contents including LittleFS, NVS, and firmware:

```bash
python3 -m esptool --chip esp32s3 --port /dev/cu.usbmodem* erase-flash
```

Then reflash the firmware. A new Reticulum identity will be generated on first boot. All settings, messages, and contacts stored in flash are lost. SD card data is preserved.

### Settings-only reset

In Settings → About → Factory Reset: clears the user config JSON from flash and SD, then reboots. Radio, WiFi, display, and audio revert to compile-time defaults. Identity and messages are preserved.

---

## Diagnostic Reference

### Serial log tags

Every subsystem logs with a tag prefix for easy filtering:

| Tag | Subsystem |
|-----|-----------|
| `[BOOT]` | Setup sequence |
| `[RADIO]` | SX1262 driver |
| `[LORA_IF]` | LoRa ↔ Reticulum interface |
| `[WIFI]` | WiFi AP/STA |
| `[TCP]` | TCP client connections |
| `[LXMF]` | LXMF message protocol |
| `[SD]` | SD card storage |
| `[HOTKEY]` | Keyboard hotkey dispatch |
| `[TEST]` | Radio test packet (Ctrl+T) |
| `[RSSI]` | RSSI monitor (Ctrl+R) |
| `[AUTO]` | Periodic auto-announce |
| `[BLE]` | BLE stub status |

### Ctrl+D diagnostic fields

| Field | Meaning |
|-------|---------|
| Identity | 16-byte Reticulum destination hash (hex) |
| Transport | ACTIVE or OFFLINE |
| Paths / Links | Number of known Reticulum paths and active links |
| Freq / SF / BW / CR / TXP | Current radio parameters |
| Preamble | Preamble length in symbols |
| SyncWord regs | Raw 0x0740/0x0741 register values (should be 0x14/0x24 for Reticulum) |
| DevErrors | SX1262 error register (0x0040 = PLL lock failure) |
| Status | SX1262 chip mode and command status |
| Current RSSI | Instantaneous RSSI in dBm (noise floor ~-110 to -120 dBm) |
| Free heap | Available RAM in bytes (typical: ~120–150 KB) |
| Flash | LittleFS used/total bytes |
| Uptime | Seconds since boot |
