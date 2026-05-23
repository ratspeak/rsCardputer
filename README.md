<div align="center">

# rsCardputer

**Dual-mode Ratspeak firmware for the M5Stack Cardputer Adv.**

[![Status](https://img.shields.io/badge/status-beta-yellow.svg)](#status)
[![Model](https://img.shields.io/badge/model-Cardputer%20Adv-success.svg)](https://docs.m5stack.com/en/core/Cardputer-Adv)
[![License](https://img.shields.io/badge/license-mixed-blue.svg)](#license)

[Ratspeak](https://github.com/ratspeak/Ratspeak) |
[rsReticulum](https://github.com/ratspeak/rsReticulum) |
[rsLXMF](https://github.com/ratspeak/rsLXMF) |
[Downloads](https://ratspeak.org/download.html)

</div>

---

rsCardputer turns [M5Stack's Cardputer Adv](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3?srsltid=AfmBOoqkAKYv165WC27WbB4bj73CxxMa12pImKTrkvyk9ZtqFUb4_gPt$0) into a local two-mode
Reticulum device. On boot, the launcher lets you choose between:

- **Standalone**: a Reticulum/LXMF messenger that runs entirely on the
  Cardputer Adv.
- **RNode**: a host-controlled RNode-style interface for Ratspeak,
  Sideband, or other Reticulum clients over BLE or USB serial.

Both firmwares live in internal ESP32-S3 flash, with support for SD in the future to reduce flash usage. There are individual releases available for just Standalone or just RNode as well for those using pre-existing flashers or wanting just one firmware.

## Contents

- [Hardware](#hardware)
- [Flashing](#flashing)
- [Radio Presets](#radio-presets)
- [Standalone Mode](#standalone-mode)
- [RNode Mode](#rnode-mode)
- [Build From Source](#build-from-source)
- [License](#license)

## Hardware

The supported Cardputer Adv has:

- ESP32-S3FN8
- 8 MB internal flash
- Built-in keyboard and 240x135 display
- microSD support (32GB max), optional but recommended
- **SX1262 LoRa radio***

*LoRa capabilities are only possible with the Cardputer Adv LoRa cap (module), which they usually sell separately. Without it, the Cardputer will only have WiFi/BLE capabilities.

## Flashing

For flashing with a pre-built firmware, use [Ratspeak's flasher](https://ratspeak.org/download.html) in a supported browser, and flash the device in just a few clicks.

To flash locally, connect the Cardputer Adv over USB and find
the serial port:

```bash
ls /dev/cu.usbmodem*      # macOS
ls /dev/ttyACM*           # Linux
```

Then flash the merged image:

```bash
cd rsCardputer
make flash port=/dev/cu.usbmodem3101
```

## Radio Presets

| Preset | SF | BW | CR | TXP | Bitrate | Link budget |
|---|---|---|---|---|---|---|
| Short Turbo | 7 | 500 kHz | 4/5 | 14 dBm | 21.99 kbps | 140 dB |
| Short Fast | 7 | 250 kHz | 4/5 | 14 dBm | 10.84 kbps | 143 dB |
| Short Slow | 8 | 250 kHz | 4/5 | 14 dBm | 6.25 kbps | 145.5 dB |
| Medium Fast | 9 | 250 kHz | 4/5 | 17 dBm | 3.52 kbps | 148 dB |
| Medium Slow | 10 | 250 kHz | 4/5 | 17 dBm | 1.95 kbps | 150.5 dB |
| Long Turbo | 11 | 500 kHz | 4/8 | 22 dBm | 1.34 kbps | 150 dB |
| **Long Fast** *(default)* | **11** | **250 kHz** | **4/5** | **22 dBm** | **1.07 kbps** | **153 dB** |
| Long Moderate | 11 | 125 kHz | 4/8 | 22 dBm | 0.34 kbps | 156 dB |

Host clients can change the RNode radio profile through normal RNode commands, typically in-app.
Standalone mode also exposes radio settings on-device.

The supported SX1262 cap is an 850-950 MHz radio target. 868 MHz and 915 MHz
are valid software profiles for that hardware range. 433 MHz requires 433 MHz
radio hardware. You are responsible for operating within your local laws.

## Standalone Mode

Standalone mode gives the Cardputer Adv a local Reticulum
identity, LXMF messaging, LoRa operation, TCP access over WiFi,
GPS time sync, contact/message storage, and on-device settings.

On first boot, Standalone mode asks for timezone - this is so the GPS-received time shows relevant to your timezone, it is not shared with anyone.

## RNode Mode

RNode mode behaves like an RNode radio for any Reticulum client.

- USB CDC serial carries the normal RNode KISS protocol.
- BLE is enabled by default for phone/client reconnection.
- Hold `p` or `OK` for three seconds to enter BLE pairing mode.
- Hold `b` for three seconds to toggle BLE on or off.
- Pairing mode times out after 30 seconds if no client connects.
- The radio stays idle until the host client connects.

The RNode target self-provisions its EEPROM identity and firmware hash on first
boot, so users should not need a separate `rnodeconf` setup step to clear
config or firmware-corrupt warnings.

## Build From Source

rsCardputer is a single source tree. Standalone mode, the RNode target, the launcher,
partition layout, and package tooling are all included in this repository.

Install prerequisites:

- Python 3.
- PlatformIO.
- `arduino-cli`.
- ESP32 Arduino core and M5 libraries for the RNode build.

Install the Python tools:

```bash
python3 -m pip install platformio esptool
```

One-time RNode dependency setup:

```bash
make prep-cardputer_adv
```

Build all firmware images and package the release artifacts:

```bash
make package
```

Build targets are split so each component can also be worked on independently:

```bash
make build-launcher      # launcher only
make build-standalone    # standalone messenger app
make build-rnode         # host-controlled RNode target
make full-image          # launcher + Standalone + RNode
make standalone-image    # standalone merged image
make rnode-only-image    # standalone RNode merged image
```

Release artifacts are written to `dist/`:

```text
dist/rscardputer-full.zip
dist/rscardputer-standalone.zip
dist/rscardputer-rnode.zip
dist/rscardputer-standalone-m5launcher.bin
dist/rscardputer-rnode-m5launcher.bin
```

Use the `.zip` files with the Ratspeak web flasher. Each zip contains a merged
factory image and the web-flasher manifest for the Cardputer Adv. Raw `.bin`
files under `build/` are internal build outputs.

The `*-m5launcher.bin` files are app images for M5Launcher users who already
boot through bmorcelli's Launcher and want to start Standalone or RNode directly
from SD, without using the bundled rsCardputer launcher.

Flash the bundled launcher image with:

```bash
make flash port=/dev/cu.usbmodem3101
```

CI uses the same `make package` flow and publishes the bundled image plus both
individual firmware images.

## License & Contribution

Ratspeak's Cardputer standalone application, launcher, partition tables, and packaging tools are licensed under the GNU Affero General Public License v3.0 or later. See [LICENSE](LICENSE).

The bundled RNode firmware under `vendor/rnode_firmware/` is licensed under
the GNU General Public License v3.0. See [LICENSE-RNODE](LICENSE-RNODE).

Standalone mode uses a [custom fork](https://github.com/ratspeak/microReticulum) of microReticulum, which was originally written by [Chad Atterman](https://github.com/attermann).
