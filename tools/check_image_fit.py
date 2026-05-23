#!/usr/bin/env python3
"""Check that launcher, Ratcom, and RNode app images fit the shared slots."""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_int(value: str) -> int:
    return int(value, 0)


def image_size(path: Path) -> int:
    if not path.exists():
        raise FileNotFoundError(path)
    return path.stat().st_size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--launcher", required=True, type=Path)
    parser.add_argument("--launcher-slot-size", default="0x100000", type=parse_int)
    parser.add_argument("--ratcom", required=True, type=Path)
    parser.add_argument("--rnode", required=True, type=Path)
    parser.add_argument("--app-slot-size", default="0x260000", type=parse_int)
    args = parser.parse_args()

    failed = False
    checks = (
        ("Launcher", args.launcher, args.launcher_slot_size),
        ("Ratcom", args.ratcom, args.app_slot_size),
        ("RNode", args.rnode, args.app_slot_size),
    )

    for name, path, slot_size in checks:
        size = image_size(path)
        margin = slot_size - size
        print(f"{name}: {size} bytes, slot {slot_size} bytes, margin {margin} bytes")
        if margin < 0:
            failed = True

    if failed:
        print("error: at least one image is too large for the shared OTA slot")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
