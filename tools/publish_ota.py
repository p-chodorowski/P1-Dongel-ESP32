#!/usr/bin/env python3
"""Stage Ultra OTA files locally: version-manifest.json and *_8Mb.bin.

Does not upload. A developer copies the output directory to the HTTP host.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION_RE = re.compile(r"#define\s+_VERSION_(MAJOR|MINOR|PATCH|FORK)\s+(\d+)")


@dataclass(frozen=True)
class PublishResult:
    manifest_path: Path
    firmware_path: Path | None


def parse_version(version_h_text: str) -> tuple[int, int, int, int]:
    found: dict[str, int] = {}
    for match in VERSION_RE.finditer(version_h_text):
        found[match.group(1).lower()] = int(match.group(2))
    try:
        return found["major"], found["minor"], found["patch"], found.get("fork", 0)
    except KeyError as exc:
        raise ValueError("version.h is missing _VERSION_MAJOR/MINOR/PATCH") from exc


def version_manifest(major: int, minor: int, fix: int, fork: int) -> dict[str, int | str]:
    return {
        "version": f"{major}.{minor}.{fix}.{fork}",
        "major": major,
        "minor": minor,
        "fix": fix,
        "fork": fork,
    }


def dest_bin_name(version: str) -> str:
    return f"DSMR-API-V{version}_8Mb.bin"


def publish(
    version_h: Path,
    firmware: Path | None,
    out_dir: Path,
) -> PublishResult:
    major, minor, fix, fork = parse_version(version_h.read_text(encoding="utf-8"))
    manifest = version_manifest(major, minor, fix, fork)
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = out_dir / "version-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=1) + "\n", encoding="utf-8")

    firmware_path = None
    if firmware is not None:
        firmware_path = out_dir / dest_bin_name(str(manifest["version"]))
        shutil.copy2(firmware, firmware_path)

    return PublishResult(manifest_path=manifest_path, firmware_path=firmware_path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Stage Ultra OTA manifest and 8MB firmware name (no upload)."
    )
    parser.add_argument(
        "--version-h",
        type=Path,
        default=ROOT / "version.h",
        help="Path to version.h (default: repo version.h)",
    )
    parser.add_argument(
        "--firmware",
        type=Path,
        help="Compiled Ultra .bin to copy as DSMR-API-V{version}_8Mb.bin",
    )
    parser.add_argument(
        "--out",
        type=Path,
        required=True,
        help="Output directory for version-manifest.json and the renamed bin",
    )
    args = parser.parse_args()
    result = publish(version_h=args.version_h, firmware=args.firmware, out_dir=args.out)
    print(result.manifest_path)
    if result.firmware_path is not None:
        print(result.firmware_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
