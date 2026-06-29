<div align="center">

# rsCardputer

**Dual-mode Ratspeak firmware for the M5Stack Cardputer Adv.**

[![Status](https://img.shields.io/badge/status-beta-yellow.svg)](#install)
[![Model](https://img.shields.io/badge/model-Cardputer%20Adv-success.svg)](https://docs.m5stack.com/en/core/Cardputer-Adv)
[![Version](https://img.shields.io/badge/version-2.0.3-success.svg)](https://github.com/ratspeak/rsCardputer/releases)
[![License](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue.svg)](LICENSE)

[Ratspeak](https://github.com/ratspeak/Ratspeak) |
[Docs](https://ratspeak.org/docs.html) |
[Downloads](https://ratspeak.org/download.html) |
[rsReticulum](https://github.com/ratspeak/rsReticulum)

</div>

---

rsCardputer turns the
[M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/Cardputer-Adv) into a
two-mode Reticulum handheld. Standalone mode is an on-device Ratspeak/LXMF
messenger. RNode mode makes the Cardputer Adv a host-controlled radio for
Ratspeak, Sideband, or another Reticulum client over BLE or USB serial.

## Hardware

The supported target is the **M5Stack Cardputer Adv** with:

- ESP32-S3FN8.
- 8 MB internal flash.
- Built-in keyboard and 240x135 display.
- microSD slot.
- SX1262 LoRa cap/module.

LoRa requires the Cardputer Adv LoRa cap. Without it, the device can still run
the app with WiFi/BLE features, but it cannot transmit or receive LoRa.

## Install

Use the Ratspeak web flasher:
[ratspeak.org/download.html](https://ratspeak.org/download.html).

Connect the Cardputer Adv over USB, put it in download mode if needed, then
flash `rscardputer-full`. The standalone-only and RNode-only images are release
artifacts for launcher users or focused testing.

## Modes

On boot, the launcher lets you choose:

- **Standalone**: a local Reticulum/LXMF messenger with identity management,
  contacts, peer discovery, messages, LoRa, and WiFi TCP access.
- **RNode**: a host-controlled RNode-style radio target for Ratspeak or other
  Reticulum clients over BLE or USB serial.

RNode mode self-provisions the Cardputer Adv RNode product/model/default config
and running firmware hash on first boot, so users should not need a separate
`rnodeconf` setup step for the bundled release images.

## Basic Use

On first boot, Standalone mode generates a Reticulum identity and asks for a
timezone so GPS-provided time can be displayed locally. Your LXMF address is the
32-character hex string you share with contacts.

- Navigation: built-in keyboard and OK/Enter.
- BLE pairing in RNode mode: hold `p` or `OK` for three seconds.
- BLE toggle in RNode mode: hold `b` for three seconds.
- The RNode radio stays idle until a host client connects and configures it.

## Radio Presets

`Long Fast` is the compiled-in default. Host clients can change RNode radio
parameters through normal RNode commands, and Standalone mode exposes radio
settings on-device.

| Preset | SF | BW | CR | TXP | Bitrate | Link budget |
|---|---:|---:|---:|---:|---:|---:|
| Short Turbo | 7 | 500 kHz | 4/5 | 14 dBm | 21.99 kbps | 140 dB |
| Short Fast | 7 | 250 kHz | 4/5 | 14 dBm | 10.84 kbps | 143 dB |
| Short Slow | 8 | 250 kHz | 4/5 | 14 dBm | 6.25 kbps | 145.5 dB |
| Medium Fast | 9 | 250 kHz | 4/5 | 17 dBm | 3.52 kbps | 148 dB |
| Medium Slow | 10 | 250 kHz | 4/5 | 17 dBm | 1.95 kbps | 150.5 dB |
| Long Turbo | 11 | 500 kHz | 4/8 | 22 dBm | 1.34 kbps | 150 dB |
| **Long Fast** *(default)* | **11** | **250 kHz** | **4/5** | **22 dBm** | **1.07 kbps** | **153 dB** |
| Long Moderate | 11 | 125 kHz | 4/8 | 22 dBm | 0.34 kbps | 156 dB |

The supported SX1262 cap is an 850-950 MHz radio target. 868 MHz and 915 MHz
are valid software profiles for that hardware range. 433 MHz requires 433 MHz
radio hardware. You are responsible for operating within local laws and radio
regulations.

## Build From Source

```bash
git clone https://github.com/ratspeak/rsCardputer
cd rsCardputer
python3 -m pip install platformio esptool
make prep-cardputer_adv
make package
make flash port=/dev/cu.usbmodem3101
```

Useful build targets:

```bash
make build-launcher      # launcher only
make build-standalone    # standalone messenger app
make build-rnode         # host-controlled RNode target
make full-image          # launcher + Standalone + RNode
make standalone-image    # standalone merged image
make rnode-only-image    # standalone RNode merged image
make package             # release zips and launcher bins
```

Release artifacts are written to `dist/`:

```text
dist/rscardputer-full.zip
dist/rscardputer-standalone.zip
dist/rscardputer-rnode.zip
dist/rscardputer-standalone-m5launcher.bin
dist/rscardputer-rnode-m5launcher.bin
```

Use the `.zip` files with the Ratspeak web flasher. The `*-m5launcher.bin`
files are app images for M5Launcher/M5Burner-style launchers that boot
Standalone or RNode directly from SD.

## License

rsCardputer standalone firmware, launcher, partition tables, and packaging
tools are licensed under the GNU Affero General Public License v3.0 or later.
See [LICENSE](LICENSE).

The bundled RNode firmware under `vendor/rnode_firmware/` is licensed under the
GNU General Public License v3.0. See [LICENSE-RNODE](LICENSE-RNODE).

Standalone mode uses a
[custom fork](https://github.com/ratspeak/microReticulum) of microReticulum,
which was originally written by
[Chad Atterman](https://github.com/attermann).
