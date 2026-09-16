#!/usr/bin/env python3
"""Sanity checks for the Ultra compile helper (does not invoke arduino-cli)."""

from __future__ import annotations

import unittest
from pathlib import Path

import compile_ultra
from publish_ota import dest_bin_name


class CompileUltraConfigTests(unittest.TestCase):
    def test_fqbn_is_s3_8mb_default_8mb(self) -> None:
        self.assertIn("esp32s3", compile_ultra.FQBN)
        self.assertIn("FlashSize=8M", compile_ultra.FQBN)
        self.assertIn("PartitionScheme=default_8MB", compile_ultra.FQBN)

    def test_cli_config_exists(self) -> None:
        self.assertTrue(compile_ultra.CLI_CONFIG.is_file())

    def test_workflow_exists(self) -> None:
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "compile-ultra.yml"
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("compile_ultra.py", text)
        self.assertIn("upload-artifact", text)
        self.assertNotIn("ftp", text.lower())

    def test_staged_name_is_8mb(self) -> None:
        self.assertEqual(dest_bin_name("5.8.4.1"), "DSMR-API-V5.8.4.1_8Mb.bin")

    def test_registry_libs_do_not_include_esptelnet(self) -> None:
        self.assertNotIn("ESPTelnet", compile_ultra.REGISTRY_LIBS)
        self.assertIn("TelnetStream", compile_ultra.REGISTRY_LIBS)

    def test_git_libs_include_asynctcp_for_emodbus(self) -> None:
        self.assertTrue(any("AsyncTCP" in url for url in compile_ultra.GIT_LIBS))


if __name__ == "__main__":
    unittest.main()
