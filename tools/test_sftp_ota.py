#!/usr/bin/env python3
"""Tests for the Ultra SFTP publisher (no network)."""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import sftp_ota


def argparse_namespace(**overrides: object) -> object:
    values = {
        "host": None,
        "user": None,
        "path": None,
        "test_path": None,
        "port": None,
        "identity": None,
        "channel": "production",
        "dry_run": False,
    }
    values.update(overrides)
    return type("NS", (), values)()


class SftpOtaTests(unittest.TestCase):
    def test_find_ota_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp)
            (stage / "version-manifest.json").write_text("{}\n", encoding="utf-8")
            bin_path = stage / "DSMR-API-V5.9.5.1_8Mb.bin"
            bin_path.write_bytes(b"fw")
            found = sftp_ota.find_ota_files(stage)
            self.assertEqual(found.firmware, bin_path)
            self.assertEqual(found.manifest.name, "version-manifest.json")

    def test_find_ota_files_requires_both(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp)
            (stage / "version-manifest.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaises(SystemExit):
                sftp_ota.find_ota_files(stage)

    def test_remote_dir_channels(self) -> None:
        self.assertEqual(
            sftp_ota.remote_dir("/var/www/html/p1dongle/ultra/", "production"),
            "/var/www/html/p1dongle/ultra",
        )
        self.assertEqual(
            sftp_ota.remote_dir("/var/www/html/p1dongle/ultra", "test"),
            "/var/www/html/p1dongle/ultra/test",
        )
        self.assertEqual(
            sftp_ota.remote_dir("/var/www/html/p1dongle/ultra", "test", "/srv/ota/test"),
            "/srv/ota/test",
        )

    def test_sftp_batch_puts_bin_before_manifest(self) -> None:
        files = sftp_ota.OtaFiles(
            firmware=Path("/tmp/DSMR-API-V5.9.5.1_8Mb.bin"),
            manifest=Path("/tmp/version-manifest.json"),
        )
        batch = sftp_ota.sftp_batch(files, "/var/www/html/p1dongle/ultra")
        self.assertLess(batch.index("8Mb.bin"), batch.index("version-manifest.json"))

    def test_sftp_command_uses_key_not_argv_password(self) -> None:
        target = sftp_ota.SftpTarget(
            host="209.38.55.197",
            user="deploy",
            path="/var/www/html/p1dongle/ultra",
            port=22,
            identity=Path("/tmp/id_ota"),
            password=None,
        )
        cmd = sftp_ota.sftp_command(target, Path("/tmp/batch"))
        self.assertEqual(cmd[0], "sftp")
        self.assertIn("BatchMode=yes", cmd)
        self.assertIn("/tmp/id_ota", cmd)
        self.assertIn("deploy@209.38.55.197", cmd)
        joined = " ".join(cmd).lower()
        self.assertNotIn("ftp_user", joined)
        self.assertNotIn("-pw", cmd)

    def test_password_uses_sshpass_env_not_argv(self) -> None:
        target = sftp_ota.SftpTarget(
            host="209.38.55.197",
            user="deploy",
            path="/var/www/html/p1dongle/ultra",
            port=22,
            identity=None,
            password="secret-from-github",
        )
        cmd = sftp_ota.sftp_command(target, Path("/tmp/batch"))
        self.assertEqual(cmd[:2], ["sshpass", "-e"])
        self.assertNotIn("secret-from-github", cmd)

    def test_reject_ftp_env(self) -> None:
        with mock.patch.dict(os.environ, {"FTP_USER": "old"}, clear=False):
            with self.assertRaises(SystemExit):
                sftp_ota.reject_forbidden_env()

    def test_load_target_requires_user_and_path(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(SystemExit):
                sftp_ota.load_target(argparse_namespace())

    def test_dry_run_does_not_call_sftp(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp)
            (stage / "version-manifest.json").write_text("{}\n", encoding="utf-8")
            (stage / "DSMR-API-V5.9.5.1_8Mb.bin").write_bytes(b"fw")
            files = sftp_ota.find_ota_files(stage)
            target = sftp_ota.SftpTarget("209.38.55.197", "deploy", "/var/www/html/p1dongle/ultra", 22, None, None)
            with mock.patch("sftp_ota.subprocess.run") as run:
                sftp_ota.upload(files, target, dry_run=True)
                run.assert_not_called()

    def test_workflow_sftp_is_secret_gated(self) -> None:
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "compile-ultra.yml"
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("sftp_ota.py", text)
        self.assertIn("secrets.OTA_SFTP_HOST", text)
        self.assertIn("secrets.OTA_SFTP_USER", text)
        self.assertIn("secrets.OTA_SFTP_PATH", text)
        self.assertIn("secrets.OTA_SFTP_KEY", text)
        self.assertIn("secrets.OTA_SFTP_PASSWORD", text)
        self.assertIn("SFTP secrets not set; skip upload", text)
        self.assertNotIn("ftp://", text.lower())
        self.assertNotRegex(text, r"(?i)(BEGIN OPENSSH|BEGIN RSA) ")


if __name__ == "__main__":
    unittest.main()
