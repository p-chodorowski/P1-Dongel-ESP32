#!/usr/bin/env python3
"""Sanity checks for the Ultra compile helper (does not invoke arduino-cli)."""

from __future__ import annotations

import unittest
from pathlib import Path

import compile_ultra
from publish_ota import dest_bin_name


class CompileUltraConfigTests(unittest.TestCase):
    def test_fqbn_matches_sketch_yaml_ultra(self) -> None:
        self.assertEqual(compile_ultra.FQBN, compile_ultra.load_default_fqbn())
        self.assertIn("esp32s3", compile_ultra.FQBN)
        self.assertIn("FlashSize=8M", compile_ultra.FQBN)
        self.assertIn("PartitionScheme=default_8MB", compile_ultra.FQBN)
        self.assertIn("FlashMode=qio", compile_ultra.FQBN)
        self.assertIn("CPUFreq=240", compile_ultra.FQBN)
        self.assertIn("CDCOnBoot=default", compile_ultra.FQBN)
        self.assertNotIn("CDCOnBoot=cdc", compile_ultra.FQBN)
        self.assertIn("PSRAM=disabled", compile_ultra.FQBN)

    def test_esp32_core_matches_upstream_sdk(self) -> None:
        self.assertEqual(compile_ultra.ESP32_CORE, "esp32:esp32@3.3.11")

    def test_cli_config_exists(self) -> None:
        self.assertTrue(compile_ultra.CLI_CONFIG.is_file())
        text = compile_ultra.CLI_CONFIG.read_text(encoding="utf-8")
        self.assertIn("enable_unsafe_install: true", text)

    def test_workflow_exists(self) -> None:
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "compile-ultra.yml"
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("compile_ultra.py", text)
        self.assertIn("test_compile_ultra.py", text)
        self.assertIn("upload-artifact", text)
        self.assertIn("sftp_ota.py", text)
        self.assertNotIn("ftp://", text.lower())
        self.assertNotIn("secrets.FTP_USER", text)

    def test_staged_name_is_8mb(self) -> None:
        self.assertEqual(dest_bin_name("5.9.5.1"), "DSMR-API-V5.9.5.1_8Mb.bin")

    def test_registry_libs_do_not_include_esptelnet(self) -> None:
        self.assertNotIn("ESPTelnet", compile_ultra.REGISTRY_LIBS)
        self.assertIn("TelnetStream", compile_ultra.REGISTRY_LIBS)
        self.assertIn("WebSockets", compile_ultra.REGISTRY_LIBS)
        self.assertIn("PubSubClient", compile_ultra.REGISTRY_LIBS)

    def test_git_libs_match_sketch(self) -> None:
        joined = "\n".join(compile_ultra.GIT_LIBS)
        self.assertIn("dsmr3Lib", joined)
        self.assertNotIn("dsmr2Lib", joined)
        self.assertTrue(any("AsyncTCP" in url for url in compile_ultra.GIT_LIBS))
        self.assertTrue(any("eModbus" in url for url in compile_ultra.GIT_LIBS))
        self.assertTrue(any("micro-ecc" in url for url in compile_ultra.GIT_LIBS))
        self.assertFalse(any("ESPAsyncWebServer" in url for url in compile_ultra.GIT_LIBS))

    def test_sketch_uses_dsmr3(self) -> None:
        """mhendriks 5.9+ uses dsmr3Lib. Do not pull the old dsmr2 compat include."""
        root = Path(__file__).resolve().parents[1]
        header = (root / "DSMRloggerAPI.h").read_text(encoding="utf-8")
        self.assertIn("dsmr3.h", header)
        self.assertNotIn("P1FixedReaderCompat.h", header)


if __name__ == "__main__":
    unittest.main()
