<div align="center">

# [Ratcom](https://ratspeak.org/)

**Dual-mode Reticulum firmware for the M5Stack Cardputer Adv.**

[Ratspeak](https://github.com/ratspeak/Ratspeak) |
[rsReticulum](https://github.com/ratspeak/rsReticulum) |
[rsLXMF](https://github.com/ratspeak/rsLXMF) |
[Download](https://ratspeak.org/download.html) |
[Cardputer Adv Docs](https://docs.m5stack.com/en/core/M5Cardputer%20Adv)

[![License: AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue.svg)](LICENSE)
[![RNode: GPL-3.0](https://img.shields.io/badge/bundled%20RNode-GPL--3.0-blue.svg)](docs/LICENSING.md)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![Status](https://img.shields.io/badge/status-alpha-yellow.svg)](#current-state)

[Original demo video: Reticulum Standalone - T-Deck & Cardputer Adv](https://www.youtube.com/watch?v=F6I6fkMPxgI)

</div>

## What It Is

Ratcom turns the M5Stack Cardputer Adv into a pocket Reticulum device. It can
run as a standalone encrypted messenger, or it can act as a regular
host-controlled RNode for Ratspeak, Sideband, or another Reticulum client.

The default Cardputer Adv image is a small launcher with two firmware modes:

- **Ratcom** — standalone end-to-end encrypted LXMF messaging over LoRa, TCP
  over WiFi for bridging, node discovery, identity management, GPS time sync,
  and on-device settings.
- **RNode** — a dumb host-controlled RNode-style radio over BLE or USB serial,
  using Mark Qvist's upstream RNode firmware lineage with Cardputer Adv support.

Both modes live in internal flash. Switching modes does not require WiFi,
internet access, an SD card, or an over-the-air update service.

## Current State

Ratcom is alpha firmware. The core Cardputer Adv flow works, but this is still
hardware software: expect sharp edges, bring useful bug reports, and assume
the release process will keep improving.

The combined Cardputer Adv image is the recommended install path. Standalone
Ratcom-only and RNode-only images are built for recovery, testing, and advanced
users, but normal users should flash the full launcher image.

## What You Get

- Standalone LXMF messaging on the Cardputer Adv.
- A built-in RNode mode for existing Reticulum clients.
- A local launcher for switching between modes without reflashing.
- BLE pairing support in RNode mode.
- LoRa radio presets plus manual radio configuration.
- WiFi STA/AP plumbing for Reticulum bridging experiments.
- SD-card support for Ratcom data where available.
- Local build outputs for web flasher `.zip` files and merged `.bin` images.

## Install

Use the [Ratspeak download page](https://ratspeak.org/download.html) when
public builds are available. Put the Cardputer Adv in download mode, select the
USB device in the browser, and flash the full Cardputer Adv image.

The full image contains the launcher, Ratcom mode, and RNode mode.

> **Upgrade warning:** the dual-mode Cardputer Adv firmware uses a different
> partition layout than older standalone Ratcom builds. Treat it as a full
> firmware upgrade and expect Ratcom data/config stored in internal flash to be
> reset. SD-card data under `/ratcom/` may remain present, but migration is not
> guaranteed or assisted.

## Build From Source

Install PlatformIO and Arduino CLI first. Then build the complete firmware
family:

```bash
git clone https://github.com/ratspeak/ratcom
cd ratcom
pip install platformio
make -C vendor/rnode_firmware prep-cardputer_adv
make package
```

Flash the recommended full image:

```bash
make flash port=/dev/cu.usbmodem3101
```

Build outputs land in `dist/`:

| Artifact | Purpose |
| --- | --- |
| `ratcom-cardputer-adv-full.bin/.zip` | Recommended launcher + Ratcom + RNode image. |
| `ratcom-cardputer-adv-ratcom-only.bin/.zip` | Standalone Ratcom image for recovery/debugging. |
| `ratcom-cardputer-adv-rnode-only.bin/.zip` | Standalone RNode image for recovery/debugging. |

The `.bin` files are merged factory images flashed at offset `0x0`. The `.zip`
files include the same merged image plus a web-flasher manifest.

## Using It

The launcher is the first screen after flashing or power-on. Select Ratcom or
RNode and press Enter. If you do nothing, it starts the last-used mode after a
short timeout.

### Ratcom Mode

On first boot, Ratcom asks for a timezone and display name. Your LXMF address,
which you share with contacts, is shown on the Home tab.

Navigate with the Cardputer keyboard:

- **Home** — local identity, radio state, announce action, and current radio
  profile.
- **Msgs** — local conversations and message entry.
- **Peers** — discovered LXMF peers.
- **Settings** — radio, WiFi, SD card, display, audio, about, and reset tools.

Press Enter on the Home tab to announce your identity to nearby peers.

The current radio presets are:

| Preset | SF | BW | CR | TX Power |
| --- | --- | --- | --- | --- |
| Short Turbo | 7 | 500 kHz | 4/5 | 14 dBm |
| Short Fast | 7 | 250 kHz | 4/5 | 14 dBm |
| Short Slow | 8 | 250 kHz | 4/5 | 14 dBm |
| Medium Fast | 9 | 250 kHz | 4/5 | 17 dBm |
| Medium Slow | 10 | 250 kHz | 4/5 | 17 dBm |
| Long Turbo | 11 | 500 kHz | 4/8 | 22 dBm |
| Long Fast | 11 | 250 kHz | 4/5 | 22 dBm |
| Long Moderate | 11 | 125 kHz | 4/8 | 22 dBm |

All radio parameters are individually tunable. Changes apply immediately, no
reboot required. You are responsible for operating within the laws and
requirements that apply in your jurisdiction.

### RNode Mode

RNode mode behaves like a host-controlled radio, not a standalone messenger.

- USB CDC serial carries the normal RNode KISS protocol where host support
  allows it.
- BLE is enabled for phone/client pairing.
- Hold `p` or Enter/OK for three seconds to enter BLE pairing mode.
- Pairing mode times out after 30 seconds if no client connects.
- Host clients can change the RNode radio profile through normal RNode
  commands.

RNode mode self-provisions its local configuration and firmware hash on first
boot, so normal users should not need a separate `rnodeconf` step just to clear
"not provisioned" or "firmware corrupt" warnings.

## Platform Notes

- Android USB-OTG behavior is still being investigated with Ratspeak and
  rsReticulum; BLE is the current phone-friendly RNode path.
- Ratcom WiFi bridging is alpha and will continue to change with the Ratspeak
  client release.
- The Cardputer Adv has limited internal flash. The full image is laid out as
  launcher + Ratcom + RNode slots with a shared data partition.

## License

GNU Affero General Public License v3.0 or later for Ratcom. The bundled RNode
firmware is GPL-3.0. See [LICENSE](LICENSE), [docs/LICENSING.md](docs/LICENSING.md),
and component notices.

Vendored third-party code keeps its own license notices, including
`lib/Crypto` under MIT. Radio-driver excerpts attributed in source remain under
their original MIT notice.
