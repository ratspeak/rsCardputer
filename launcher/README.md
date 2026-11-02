# Ratspeak Launcher

This is the legacy rsCardputer mode chooser for Cardputer Adv. For current
firmware, see [Ratspeak Handheld](https://github.com/ratspeak/ratspeak-handheld).

The launcher selects between the Standalone and RNode app partitions already
flashed into internal storage. It does not download or install firmware.

Controls in this repository:

- `Enter`: start selected mode.
- `;`, `,`, `W`: select Standalone.
- `.`, `/`, `S`: select RNode.
- `R`: start Standalone.
- `N`: start RNode.

If no key is pressed, the launcher starts the last-used mode after seven
seconds. Any keyboard input cancels auto-start for that boot.
