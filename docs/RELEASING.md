# Ratcom — Release Protocol

Ratcom Cardputer Adv releases are dual-mode firmware releases. The default
artifact is the full launcher image with Ratcom and RNode modes bundled.

## Pre-Release Checklist

1. All changes committed and pushed to `main`
2. Local package succeeds: `make package`
3. Flash and test on device — confirm launcher, Ratcom mode, and RNode mode boot
4. Version bumped in `src/config/Config.h` (all 4 defines: `RATCOM_VERSION_MAJOR`, `RATCOM_VERSION_MINOR`, `RATCOM_VERSION_PATCH`, `RATCOM_VERSION_STRING`)

## Release Steps

```bash
# 1. Commit version bump
git add src/config/Config.h
git commit -m "vX.Y.Z: description of changes"

# 2. Push to main
git push origin main

# 3. Create and push tag
git tag vX.Y.Z
git push origin vX.Y.Z

# CI automatically builds and creates GitHub release artifacts
```

## Post-Release Verification

1. Check [GitHub Actions](https://github.com/ratspeak/ratcom/actions) — both build and release jobs should pass
2. Verify the [release page](https://github.com/ratspeak/ratcom/releases) has these artifacts attached:
   - `ratcom-cardputer-adv-full.bin`
   - `ratcom-cardputer-adv-full.zip`
   - `ratcom-cardputer-adv-ratcom-only.bin`
   - `ratcom-cardputer-adv-ratcom-only.zip`
   - `ratcom-cardputer-adv-rnode-only.bin`
   - `ratcom-cardputer-adv-rnode-only.zip`
3. Download the full ZIP and confirm it contains:
   - `ratcom-cardputer-adv-full.bin`
   - `manifest.json`
4. Web flasher at [ratspeak.org/download](https://ratspeak.org/download.html) should present the full image as the default Cardputer Adv firmware

## Hotfix Protocol

For critical bugs in a released version:

1. Fix on `main` branch
2. Bump patch version (e.g., 1.5.6 → 1.5.7)
3. Follow normal release steps above

## Build Environment Reference

| Parameter | Value |
|-----------|-------|
| PlatformIO env | `ratcom_915` |
| Version defines | `RATCOM_VERSION_MAJOR/MINOR/PATCH/STRING` |
| Primary firmware artifact | `ratcom-cardputer-adv-full.zip` |
| Flash size | 8MB |
| Flash mode | dio |

## Upgrade Warning

The dual-mode Cardputer Adv firmware uses a different partition table than
older standalone Ratcom builds. Treat it as a full firmware upgrade and warn
users that flash-backed Ratcom identity/config/messages may reset. SD-card data
under `/ratcom/` may remain present, but migration is not guaranteed or
assisted.
