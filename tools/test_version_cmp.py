#!/usr/bin/env python3
"""Compile and run four-part OTA version compare tests."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = Path(__file__).resolve().parent / "test_version_cmp.cpp"


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        binary = Path(tmp) / "test_version_cmp"
        subprocess.run(
            [
                "g++",
                "-std=c++17",
                "-Wall",
                "-Werror",
                str(SRC),
                "-o",
                str(binary),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(binary)], check=True, cwd=ROOT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
