#!/usr/bin/env python3
"""Build a merged Cardputer Adv image with launcher, Ratcom, and RNode."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


LAUNCHER_OFFSET = "0x10000"
RATCOM_OFFSET = "0x110000"
RNODE_OFFSET = "0x370000"


def parse_int(value: str) -> int:
    return int(value, 0)


def require_file(path: Path, label: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{label} not found: {path}")
    if not path.is_file():
        raise FileNotFoundError(f"{label} is not a file: {path}")


def check_slot(path: Path, label: str, slot_size: int) -> None:
    size = path.stat().st_size
    if size > slot_size:
        raise ValueError(f"{label} image is {size} bytes, exceeds slot size {slot_size}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bootloader", required=True, type=Path)
    parser.add_argument("--partitions", required=True, type=Path)
    parser.add_argument("--boot-app0", required=True, type=Path)
    parser.add_argument("--launcher", required=True, type=Path)
    parser.add_argument("--ratcom", required=True, type=Path)
    parser.add_argument("--rnode", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--launcher-slot-size", default="0x100000", type=parse_int)
    parser.add_argument("--app-slot-size", default="0x260000", type=parse_int)
    args = parser.parse_args()

    for label, path in (
        ("bootloader", args.bootloader),
        ("partition table", args.partitions),
        ("boot_app0", args.boot_app0),
        ("launcher app", args.launcher),
        ("Ratcom app", args.ratcom),
        ("RNode app", args.rnode),
    ):
        require_file(path, label)

    check_slot(args.launcher, "Launcher", args.launcher_slot_size)
    check_slot(args.ratcom, "Ratcom", args.app_slot_size)
    check_slot(args.rnode, "RNode", args.app_slot_size)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp32s3",
        "merge-bin",
        "--flash-mode",
        "dio",
        "--flash-size",
        "8MB",
        "--output",
        str(args.output),
        "0x0000",
        str(args.bootloader),
        "0x8000",
        str(args.partitions),
        "0xe000",
        str(args.boot_app0),
        LAUNCHER_OFFSET,
        str(args.launcher),
        RATCOM_OFFSET,
        str(args.ratcom),
        RNODE_OFFSET,
        str(args.rnode),
    ]
    subprocess.run(cmd, check=True)
    print(f"dual firmware image written to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
