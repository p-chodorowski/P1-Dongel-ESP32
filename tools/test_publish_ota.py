#!/usr/bin/env python3
"""Tests for the local Ultra OTA packager (no upload)."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from publish_ota import dest_bin_name, parse_version, publish, version_manifest


VERSION_H = """#pragma once
#define _VERSION_MAJOR 5
#define _VERSION_MINOR 8
#define _VERSION_PATCH 4
"""


class PublishOtaTests(unittest.TestCase):
    def test_parse_version_from_version_h(self) -> None:
        self.assertEqual(parse_version(VERSION_H), (5, 8, 4))

    def test_manifest_fields(self) -> None:
        self.assertEqual(
            version_manifest(5, 8, 4),
            {"version": "5.8.4", "major": 5, "minor": 8, "fix": 4},
        )

    def test_ultra_bin_name_is_8mb(self) -> None:
        self.assertEqual(dest_bin_name("5.8.4"), "DSMR-API-V5.8.4_8Mb.bin")

    def test_publish_writes_manifest_and_copies_8mb_bin(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            version_h = tmp_path / "version.h"
            version_h.write_text(VERSION_H, encoding="utf-8")
            firmware = tmp_path / "sketch.bin"
            firmware.write_bytes(b"ultra-firmware")
            out = tmp_path / "stage"

            result = publish(version_h=version_h, firmware=firmware, out_dir=out)

            manifest_path = out / "version-manifest.json"
            bin_path = out / "DSMR-API-V5.8.4_8Mb.bin"
            self.assertEqual(result.manifest_path, manifest_path)
            self.assertEqual(result.firmware_path, bin_path)
            self.assertEqual(json.loads(manifest_path.read_text(encoding="utf-8")), {
                "version": "5.8.4",
                "major": 5,
                "minor": 8,
                "fix": 4,
            })
            self.assertEqual(bin_path.read_bytes(), b"ultra-firmware")

    def test_publish_manifest_only_without_firmware(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            version_h = tmp_path / "version.h"
            version_h.write_text(VERSION_H, encoding="utf-8")
            out = tmp_path / "stage"

            result = publish(version_h=version_h, firmware=None, out_dir=out)

            self.assertTrue((out / "version-manifest.json").is_file())
            self.assertIsNone(result.firmware_path)
            self.assertEqual(list(out.glob("*.bin")), [])


if __name__ == "__main__":
    unittest.main()
