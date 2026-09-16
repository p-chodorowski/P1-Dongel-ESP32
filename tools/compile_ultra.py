#!/usr/bin/env python3
"""Compile Ultra firmware (ESP32-S3, 8MB / default_8MB) and stage OTA files.

Does not upload or USB-flash. GitHub Actions runs this and artifacts the output.

  python3 tools/compile_ultra.py --out dist/ultra
  python3 tools/compile_ultra.py --out dist/ultra-test --test-channel
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from publish_ota import dest_bin_name, parse_version, publish

ROOT = Path(__file__).resolve().parents[1]
CLI_CONFIG = Path(__file__).resolve().parent / "arduino-cli.yaml"
SKETCH = ROOT
FQBN = "esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=cdc,PSRAM=disabled"
ESP32_CORE = "esp32:esp32@3.3.10"

REGISTRY_LIBS = (
    "ArduinoJson",
    "Time",
    "TelnetStream",
    "WiFiManager",
    "CRC32",
    "PubSubClient",
    "WebSockets",
)

GIT_LIBS = (
    "https://github.com/mhendriks/dsmr2Lib.git",
    "https://github.com/eModbus/eModbus.git",
    "https://github.com/kmackay/micro-ecc.git",
    "https://github.com/ESP32Async/AsyncTCP.git",
)


def arduino_cli(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    cmd = ["arduino-cli", "--config-file", str(CLI_CONFIG), *args]
    print("+", " ".join(cmd), flush=True)
    return subprocess.run(cmd, check=check, cwd=ROOT, text=True)


def ensure_arduino_cli() -> None:
    if shutil.which("arduino-cli") is None:
        raise SystemExit(
            "arduino-cli not found. Install it or run this from the Compile Ultra GitHub Action."
        )


def install_core_and_libs() -> None:
    arduino_cli("core", "update-index")
    arduino_cli("lib", "update-index")
    installed = arduino_cli("core", "install", ESP32_CORE, check=False)
    if installed.returncode != 0:
        print(f"Pin {ESP32_CORE} unavailable, installing latest esp32:esp32", flush=True)
        arduino_cli("core", "install", "esp32:esp32")
    for lib in REGISTRY_LIBS:
        arduino_cli("lib", "install", lib)
    for url in GIT_LIBS:
        arduino_cli("lib", "install", "--git-url", url)


def compile_sketch(build_dir: Path, test_channel: bool) -> Path:
    build_dir.mkdir(parents=True, exist_ok=True)
    compile_args = [
        "compile",
        "--fqbn",
        FQBN,
        "--output-dir",
        str(build_dir),
        str(SKETCH),
    ]
    if test_channel:
        compile_args[1:1] = [
            "--build-property",
            "compiler.cpp.extra_flags=-DOTA_TEST_CHANNEL",
            "--build-property",
            "compiler.c.extra_flags=-DOTA_TEST_CHANNEL",
        ]
    arduino_cli(*compile_args)
    bins = sorted(build_dir.glob("*.ino.bin")) + sorted(build_dir.glob("*.bin"))
    bins = [p for p in bins if "bootloader" not in p.name and "partitions" not in p.name]
    if not bins:
        raise SystemExit(f"No firmware .bin in {build_dir}")
    return bins[0]


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile Ultra OTA firmware (no upload).")
    parser.add_argument("--out", type=Path, required=True, help="Directory for staged manifest + 8MB bin")
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / "build" / "ultra",
        help="arduino-cli --output-dir (default: build/ultra)",
    )
    parser.add_argument(
        "--test-channel",
        action="store_true",
        help="Bake OTA_TEST_CHANNEL (http://209.38.55.197/p1dongle/ultra/test/)",
    )
    parser.add_argument(
        "--skip-install",
        action="store_true",
        help="Skip core/library install (already provisioned)",
    )
    parser.add_argument(
        "--firmware-only",
        type=Path,
        help="Skip compile; stage this existing .bin with publish_ota.py",
    )
    args = parser.parse_args()

    if args.firmware_only:
        firmware = args.firmware_only
    else:
        ensure_arduino_cli()
        if not args.skip_install:
            install_core_and_libs()
        firmware = compile_sketch(args.build_dir, args.test_channel)

    result = publish(version_h=ROOT / "version.h", firmware=firmware, out_dir=args.out)
    major, minor, fix, fork = parse_version((ROOT / "version.h").read_text(encoding="utf-8"))
    version = f"{major}.{minor}.{fix}.{fork}"
    print(f"Staged {result.manifest_path}")
    print(f"Staged {result.firmware_path} ({dest_bin_name(version)})")
    if args.test_channel:
        print("OTA URL baked in: http://209.38.55.197/p1dongle/ultra/test/")
    else:
        print("OTA URL baked in: http://209.38.55.197/p1dongle/ultra/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
