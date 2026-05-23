# Ratspeak Launcher

This is the local mode chooser for Cardputer Adv. It does not download or
install firmware. It only selects between the Ratcom and RNode app partitions
already flashed into internal storage.

Current controls:

- `Enter`: start selected mode.
- `;`, `,`, `W`: select Ratcom.
- `.`, `/`, `S`: select RNode.
- `R`: start Ratcom.
- `N`: start RNode.

If no key is pressed, the launcher starts the last-used mode after seven
seconds. Any keyboard input cancels auto-start for that boot.
