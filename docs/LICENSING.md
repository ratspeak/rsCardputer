# Ratcom Licensing Notes

Ratcom Cardputer Adv releases package multiple firmware components:

- Ratcom application code is AGPL-3.0-or-later. See `LICENSE`.
- The vendored RNode firmware under `vendor/rnode_firmware/` is GPL-3.0.
- Third-party libraries keep their original notices, including `lib/Crypto`
  under MIT.

The full Cardputer Adv image combines launcher, Ratcom, and RNode app images in
one ESP32-S3 flash layout. Release artifacts should keep source availability and
license notices for all bundled components.
