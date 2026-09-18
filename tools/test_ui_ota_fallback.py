#!/usr/bin/env python3
"""UI must not fall back to the vendor v5 OTA host."""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENDOR_FALLBACK = "ota.smart-stuff.nl/v5/"


class UiOtaFallbackTests(unittest.TestCase):
    def test_dal_js_has_no_vendor_v5_fallback(self) -> None:
        text = (ROOT / "cdn" / "dal.js").read_text(encoding="utf-8")
        self.assertNotIn(VENDOR_FALLBACK, text)
        self.assertIn("ota.smart-stuff.nl/esphome/", text)

    def test_dsmrindex_js_has_no_vendor_v5_fallback(self) -> None:
        text = (ROOT / "cdn" / "DSMRindex.js").read_text(encoding="utf-8")
        self.assertNotIn(VENDOR_FALLBACK, text)


if __name__ == "__main__":
    unittest.main()
