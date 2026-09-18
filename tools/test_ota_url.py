#!/usr/bin/env python3
"""Compile profile.h OTA macros and check Ultra vs default URLs."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECK = Path(__file__).resolve().parent / "ota_url_check.c"
ULTRA_URL = "http://209.38.55.197/p1dongle/ultra/"
ULTRA_TEST_URL = "http://209.38.55.197/p1dongle/ultra/test/"
P1P_URL = "http://ota.smart-stuff.nl/p1p/v5/"
OVERRIDE_URL = "http://example.test/custom-ota/"


def compile_and_run(extra_defines: list[str]) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        binary = Path(tmp) / "ota_url_check"
        cmd = [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Werror",
            str(CHECK),
            "-o",
            str(binary),
            *extra_defines,
        ]
        subprocess.run(cmd, check=True, cwd=ROOT)
        subprocess.run([str(binary)], check=True, cwd=ROOT)


def main() -> int:
    compile_and_run(
        [
            "-DULTRA",
            f'-DEXPECTED_OTAURL="{ULTRA_URL}"',
        ]
    )
    compile_and_run(
        [
            f'-DEXPECTED_OTAURL="{P1P_URL}"',
        ]
    )
    compile_and_run(
        [
            "-DULTRA",
            f'-DOTA_BASE_URL="{OVERRIDE_URL}"',
            f'-DEXPECTED_OTAURL="{OVERRIDE_URL}"',
        ]
    )
    compile_and_run(
        [
            "-DULTRA",
            "-DOTA_TEST_CHANNEL",
            f'-DEXPECTED_OTAURL="{ULTRA_TEST_URL}"',
        ]
    )
    # Latest main enables POST_MEENT; Ultra must still use our directory.
    compile_and_run(
        [
            "-DULTRA",
            "-DPOST_MEENT",
            f'-DEXPECTED_OTAURL="{ULTRA_URL}"',
        ]
    )
    header = (ROOT / "DSMRloggerAPI.h").read_text(encoding="utf-8")
    if "http://ota.smart-stuff.nl/p1u/v5/me/" in header:
        if "#if defined(POST_MEENT) && !defined(ULTRA)" not in header:
            raise SystemExit(
                "POST_MEENT vendor OTA URL must not apply to ULTRA (use profile.h OTAURL)"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
