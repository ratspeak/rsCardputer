
<div align="center">

# rsCardputer <br> ⚠️ DEPRECATED - use [ratspeak-handheld](https://github.com/ratspeak/ratspeak-handheld) ⚠️

**Dual-mode Ratspeak firmware for the M5Stack Cardputer Adv.**

[![Status](https://img.shields.io/badge/status-beta-yellow.svg)](#install)
[![Model](https://img.shields.io/badge/model-Cardputer%20Adv-success.svg)](https://docs.m5stack.com/en/core/Cardputer-Adv)
[![Legacy version](https://img.shields.io/badge/legacy_version-2.0.3-success.svg)](https://github.com/ratspeak/rsCardputer/releases)
[![License](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue.svg)](LICENSE)

[Ratspeak](https://github.com/ratspeak/Ratspeak) |
[Current docs](https://docs.ratspeak.org/docs/hardware/handheld-guide) |
[Legacy releases](https://github.com/ratspeak/rsCardputer/releases) |
[rsReticulum](https://github.com/ratspeak/rsReticulum)

</div>

---

**Legacy repository.** Current firmware and development are in
[Ratspeak Handheld](https://github.com/ratspeak/ratspeak-handheld). The modes,
controls, radio defaults and build artifacts below describe this retained
repository; they are not the current handheld release instructions.

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
Standalone messaging over WiFi, but it cannot transmit or receive LoRa. BLE
is the host connection for RNode mode; it does not replace the LoRa radio.

## Install

For current firmware, follow the
[handheld installation guide](https://docs.ratspeak.org/docs/hardware/flashing-firmware)
and choose **Cardputer Adv** in the
[Ratspeak web flasher](https://ratspeak.org/download.html). The current full
image is `cardputer-full.bin`.

Back up your identity and messages before changing firmware or layouts. Legacy
packages from this repository use different names and storage layouts; matching
a device name does not establish that an update will preserve its data.

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

| Preset | SF | BW | CR | TXP |
|---|---:|---:|---:|---:|
| Short Turbo | 7 | 500 kHz | 4/5 | 14 dBm |
| Short Fast | 7 | 250 kHz | 4/5 | 14 dBm |
| Short Slow | 8 | 250 kHz | 4/5 | 14 dBm |
| Medium Fast | 9 | 250 kHz | 4/5 | 17 dBm |
| Medium Slow | 10 | 250 kHz | 4/5 | 17 dBm |
| Long Turbo | 11 | 500 kHz | 4/8 | 22 dBm |
| **Long Fast** *(default)* | **11** | **250 kHz** | **4/5** | **22 dBm** |
| Long Moderate | 11 | 125 kHz | 4/8 | 22 dBm |

The supported SX1262 cap is an 850-950 MHz radio target. 868 MHz and 915 MHz
are valid software profiles for that hardware range. 433 MHz requires 433 MHz
radio hardware. You are responsible for operating within local laws and radio
regulations.

## Build From Source

These commands build the retained legacy source. Install Git, Make, Python 3
with virtual-environment support, and Arduino CLI first. Use a fresh checkout:
packaging replaces files under `dist/`.

```bash
git clone https://github.com/ratspeak/rsCardputer
cd rsCardputer
git checkout 98fec8f5e2f00330d95d0c27479a13f4b358f1f9
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install platformio esptool
make prep-cardputer_adv
make package
```

After backing up the device, `make flash PORT=/dev/cu.usbmodemXXXX` builds and
writes the full legacy image at offset `0x0`. Replace the port with your device
and close any app using it. A factory write can replace settings; do not assume
that data from another firmware layout will survive.

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

Legacy artifacts are written to `dist/`:

```text
dist/rscardputer-full.zip
dist/rscardputer-standalone.zip
dist/rscardputer-rnode.zip
dist/rscardputer-standalone-m5launcher.bin
dist/rscardputer-rnode-m5launcher.bin
```

Each `.zip` contains a merged factory image and its legacy flasher manifest.
The `*-m5launcher.bin` files are bare applications for a compatible external
launcher or a correctly sized OTA slot. Do not write a bare application at
offset `0x0`.

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
