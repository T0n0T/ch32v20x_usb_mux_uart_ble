#!/usr/bin/env python3
"""Use WCH's official communication library to flash CH32 devices."""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
from pathlib import Path


DEFAULT_COMM_LIB_DIR = Path(__file__).resolve().parent / "WCH" / "CommunicationLib"


def parse_int(value: str) -> int:
    return int(value, 0)


def load_library(lib_dir: Path) -> ctypes.CDLL:
    if not lib_dir.is_dir():
        raise FileNotFoundError(f"WCH communication library directory not found: {lib_dir}")

    deps = [
        ["libusb-1.0.so.0", "libusb-1.0.so"],
        ["libhidapi-libusb.so.0", "libhidapi-libusb.so"],
        ["libhidapi-hidraw.so.0", "libhidapi-hidraw.so"],
        ["libjaylink.so.0", "libjaylink.so"],
    ]

    rtld_global = getattr(os, "RTLD_GLOBAL", 0)
    for names in deps:
        for name in names:
            dep_path = lib_dir / name
            if dep_path.exists():
                ctypes.CDLL(str(dep_path), mode=rtld_global)
                break

    lib_path = lib_dir / "libmcuupdate.so"
    if not lib_path.is_file():
        raise FileNotFoundError(f"WCH flash library not found: {lib_path}")

    return ctypes.CDLL(str(lib_path), mode=rtld_global)


def resolve_comm_lib_dir(explicit_dir: str | None) -> Path:
    candidates = []
    if explicit_dir:
        candidates.append(Path(explicit_dir).resolve())
    candidates.append(DEFAULT_COMM_LIB_DIR.resolve())
    candidates.append(
        Path("/usr/share/MRS2/MRS-linux-x64/resources/app/resources/linux/components/WCH/Others/CommunicationLib/default")
    )

    for candidate in candidates:
        if candidate.is_dir() and (candidate / "libmcuupdate.so").is_file():
            return candidate

    raise FileNotFoundError("no usable WCH communication library directory found")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Flash CH32 firmware via WCH CommunicationLib (libmcuupdate.so)."
    )
    parser.add_argument("--file", required=True, help="Firmware image path, typically .hex or .bin")
    parser.add_argument("--chip", type=parse_int, default=5, help="WCH chip id, CH32V208 uses 5")
    parser.add_argument(
        "--iface",
        type=parse_int,
        default=1,
        help="Debug interface mode: 0=1-wire, 1=2-wire",
    )
    parser.add_argument(
        "--speed",
        type=parse_int,
        default=3,
        help="Flash speed: 1=high, 2=middle, 3=low",
    )
    parser.add_argument(
        "--ops",
        type=parse_int,
        default=15,
        help="Operation bitmap: 8=erase, 4=program, 2=verify, 1=reset",
    )
    parser.add_argument(
        "--address",
        type=parse_int,
        default=0x08000000,
        help="Flash base address, CH32V208 uses the 0x08000000 alias",
    )
    parser.add_argument(
        "--comm-lib-dir",
        default=os.environ.get("WCH_COMM_LIB_DIR"),
        help="Directory containing libmcuupdate.so and its helper libraries",
    )

    args = parser.parse_args()

    image_path = Path(args.file).resolve()
    if not image_path.is_file():
        print(f"error: firmware image not found: {image_path}", file=sys.stderr)
        return 2

    try:
        comm_lib_dir = resolve_comm_lib_dir(args.comm_lib_dir)
        lib = load_library(comm_lib_dir)
    except Exception as exc:
        print(f"error: failed to load WCH communication library: {exc}", file=sys.stderr)
        return 2

    set_target_chip = lib.McuCompiler_SetTargetChip
    set_target_chip.argtypes = [ctypes.c_int, ctypes.c_int]
    set_target_chip.restype = ctypes.c_int

    flash_operation = lib.MRSFunc_FlashOperationExB
    flash_operation.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_char_p,
    ]
    flash_operation.restype = ctypes.c_int

    set_rc = set_target_chip(args.chip, args.iface)
    if set_rc != 0:
        print(
            f"error: McuCompiler_SetTargetChip failed with code {set_rc} "
            f"(chip={args.chip}, iface={args.iface})",
            file=sys.stderr,
        )
        return 1

    flash_rc = flash_operation(
        args.chip,
        args.speed,
        args.ops,
        args.address,
        str(image_path).encode("utf-8"),
    )
    if flash_rc != 0:
        print(
        f"error: MRSFunc_FlashOperationExB failed with code {flash_rc} "
        f"(chip={args.chip}, speed={args.speed}, ops={args.ops}, address=0x{args.address:08x})",
        file=sys.stderr,
        )
        return 1

    print(
        f"flashed {image_path} via WCH CommunicationLib "
        f"(chip={args.chip}, iface={args.iface}, speed={args.speed}, ops={args.ops}, "
        f"address=0x{args.address:08x}, comm_lib_dir={comm_lib_dir})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
