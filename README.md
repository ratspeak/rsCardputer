<div align="center">

# [Ratcom](https://ratspeak.org/)

**Dual-mode Reticulum firmware for the M5Stack Cardputer Adv**

</div>

Ratcom turns an [M5Stack Cardputer Adv](https://docs.m5stack.com/en/core/M5Cardputer%20Adv) into a full local [Reticulum](https://reticulum.network/) device. The default Cardputer Adv firmware now includes a small launcher with two modes:

- **Ratcom** — standalone end-to-end encrypted [LXMF](https://github.com/markqvist/LXMF) messaging over LoRa, TCP over WiFi for bridging, node discovery, identity management, GPS time sync, and on-device settings.
- **RNode** — a dumb host-controlled RNode-style radio for Ratspeak, Sideband, or another Reticulum client over BLE or USB serial.

Both modes live in internal flash. Switching modes does not require WiFi, internet, an SD card, or an over-the-air update service.

<div align="center">

---
[![Ratspeak Demo](https://img.youtube.com/vi/F6I6fkMPxgI/maxresdefault.jpg)](https://www.youtube.com/watch?v=F6I6fkMPxgI)

<sub>[▶ YouTube: Reticulum Standalone - T-Deck & Cardputer Adv](https://www.youtube.com/watch?v=F6I6fkMPxgI)</sub>

---
</div>

## Installing

The easiest way is the **[web flasher](https://ratspeak.org/download.html)** — enable download mode (hold G0 while plugging it in), select the USB device, and flash the Cardputer Adv full image.

The full image is the recommended install path. It contains the launcher, Ratcom mode, and RNode mode.

> **Upgrade warning:** the dual-mode Cardputer Adv firmware uses a different partition layout than older standalone Ratcom builds. Treat it as a full firmware upgrade and expect on-device Ratcom data/config stored in flash to be reset. SD-card data under `/ratcom/` may remain present, but migration is not guaranteed or assisted.

To build from source:

```bash
git clone https://github.com/ratspeak/ratcom
cd ratcom
pip install platformio
# Install arduino-cli separately if it is not already available.
make -C vendor/rnode_firmware prep-cardputer_adv
make package
make flash port=/dev/cu.usbmodem3101
```

## Usage

The launcher is the first screen after flashing or power-on. Select Ratcom or RNode and press Enter. If you do nothing, it starts the last-used mode after a short timeout.

### Ratcom Mode

On first boot, Ratcom asks you to pick a timezone and set a display name. Your LXMF address, which you share with contacts, is shown on the Home tab.

**Tabs:** Home, Msgs, Nodes, Settings — navigate with `,` and `/` keys.

**Manually announce:** Press Enter on the Home tab to broadcast your identity to the network.

**Add/delete contacts/messages:** Hold Enter on a conversation to add contact or delete history.

**Sending a message:** Find someone in the Nodes tab, press Enter, type your message, hit Enter to send. Messages are encrypted end-to-end with Ed25519 signatures.

**Radio presets** (Settings → Radio):
- **Long Range** — SF12, 62.5 kHz, 22 dBm. Longest distance, slow.
- **Balanced** — SF9, 125 kHz, 17 dBm. Medium distance, medium.
- **Fast** — SF7, 250 kHz, 14 dBm. Shortest distance, fast.

All radio parameters are individually tunable. Changes apply immediately, no reboot. Please operate in accordance with local laws, as you are solely responsible for knowing which regulations and requirements apply to your jurisdiction.

### WiFi Bridging (Alpha)

Use **STA mode** to connect to existing WiFi and reach remote nodes like `rns.ratspeak.org:4242`.

To bridge LoRa with Reticulum on your computer:

1. Set WiFi to **AP mode** in Settings → WiFi (creates `ratcom-XXXX`, password: `ratspeak`)
2. Connect your computer to that network
3. Add to your Reticulum config:

```ini
[[ratcom]]
  type = TCPClientInterface
  target_host = 192.168.4.1
  target_port = 4242
```

Note: WiFi bridging methods and interfaces will be revamped with Ratspeak's client release, therefore, it's unlikely AP mode works at all currently.

### RNode Mode

RNode mode behaves like a host-controlled radio, not a standalone messenger.

- USB CDC serial carries the normal RNode KISS protocol where host support allows it.
- BLE is enabled for phone/client pairing.
- Hold Enter/OK for three seconds to enter BLE pairing mode.
- Pairing mode times out after 30 seconds if no client connects.
- Host clients can change the RNode radio profile through normal RNode commands.

RNode mode self-provisions its local configuration and firmware hash on first boot, so normal users should not need a separate `rnodeconf` step just to clear "not provisioned" or "firmware corrupt" warnings.

## Release Artifacts

Ratcom Cardputer Adv releases produce three image families:

- `ratcom-cardputer-adv-full.bin/.zip` — recommended launcher + Ratcom + RNode image.
- `ratcom-cardputer-adv-ratcom-only.bin/.zip` — standalone Ratcom image for advanced recovery/debugging.
- `ratcom-cardputer-adv-rnode-only.bin/.zip` — standalone RNode image for advanced recovery/debugging.

The `.bin` files are merged factory images flashed at offset `0x0`. The `.zip` files include the same merged image plus a web-flasher manifest.

## License

GNU Affero General Public License v3.0 or later for Ratcom. The bundled RNode firmware is GPL-3.0. See [LICENSE](LICENSE), [docs/LICENSING.md](docs/LICENSING.md), and component notices.

Vendored third-party code keeps its own license notices, including
`lib/Crypto` under MIT. Radio-driver excerpts attributed in source remain under
their original MIT notice.
