#!/usr/bin/env python3
"""Guard OpenOCD flashing for CH32V208 and keep the failure mode explicit."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


OPENOCD_LIMIT_START = 0x08028000


def parse_ihex_max_address(image_path: Path) -> int | None:
    base = 0
    max_addr = None

    with image_path.open("r", encoding="ascii") as fp:
        for line_no, raw_line in enumerate(fp, start=1):
            line = raw_line.strip()
            if not line:
                continue
            if not line.startswith(":"):
                raise ValueError(f"{image_path}:{line_no}: invalid Intel HEX record")

            payload = bytes.fromhex(line[1:])
            if len(payload) < 5:
                raise ValueError(f"{image_path}:{line_no}: truncated Intel HEX record")

            length = payload[0]
            offset = (payload[1] << 8) | payload[2]
            record_type = payload[3]
            data = payload[4:-1]

            if length != len(data):
                raise ValueError(f"{image_path}:{line_no}: record length mismatch")

            if record_type == 0x00:
                if length == 0:
                    continue
                record_max = base + offset + length - 1
                max_addr = record_max if max_addr is None else max(max_addr, record_max)
            elif record_type == 0x04:
                if length != 2:
                    raise ValueError(f"{image_path}:{line_no}: invalid extended linear address record")
                base = ((data[0] << 8) | data[1]) << 16
            elif record_type == 0x01:
                break

    return max_addr


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run OpenOCD flashing with a preflight guard for CH32V208."
    )
    parser.add_argument("--image", required=True, help="Intel HEX image for OpenOCD")
    parser.add_argument("--openocd", required=True, help="OpenOCD executable path")
    parser.add_argument("--config", required=True, help="OpenOCD config file")
    parser.add_argument(
        "--command",
        required=True,
        help='OpenOCD command body, for example: program foo.hex verify reset exit',
    )
    args = parser.parse_args()

    image_path = Path(args.image).resolve()
    if not image_path.is_file():
        print(f"error: OpenOCD image not found: {image_path}", file=sys.stderr)
        return 2

    try:
        max_addr = parse_ihex_max_address(image_path)
    except Exception as exc:
        print(f"error: failed to inspect OpenOCD image: {exc}", file=sys.stderr)
        return 2

    if max_addr is not None and max_addr >= OPENOCD_LIMIT_START:
        print(
            "error: image exceeds the currently verified OpenOCD write window for this target "
            f"(max_address=0x{max_addr:08x}, first_unsupported=0x{OPENOCD_LIMIT_START:08x}).",
            file=sys.stderr,
        )
        print(
            "error: current WCH OpenOCD `wch_riscv` driver still misidentifies CH32V208 as 160KB flash "
            "and rejects addresses at or above 0x08028000. Use `make flash` or `make flash-sdi` instead.",
            file=sys.stderr,
        )
        return 1

    cmd = [args.openocd, "-f", args.config, "-c", args.command]
    result = subprocess.run(cmd, check=False)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
