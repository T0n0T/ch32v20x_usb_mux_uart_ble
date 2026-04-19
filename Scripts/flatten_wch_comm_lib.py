#!/usr/bin/env python3
"""Normalize copied WCH communication library files into a flat single-level directory."""

from __future__ import annotations

import shutil
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: flatten_wch_comm_lib.py <dir>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        print(f"error: directory not found: {root}", file=sys.stderr)
        return 2

    for entry in sorted(root.iterdir()):
        if not entry.is_symlink():
            continue
        target = entry.resolve()
        if target.parent != root or not target.exists():
            continue
        entry.unlink()
        shutil.copy2(target, entry)

    print(f"flattened symlinks under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
