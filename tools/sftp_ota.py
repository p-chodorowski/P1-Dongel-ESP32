#!/usr/bin/env python3
"""Upload staged Ultra OTA files over OpenSSH SFTP.

CI credentials come from GitHub Secrets (injected as env). Locally the same
env names or an SSH agent / identity file. Never commit keys or passwords.

  python3 tools/sftp_ota.py --dir dist/ultra --dry-run
  python3 tools/sftp_ota.py --dir dist/ultra
  python3 tools/sftp_ota.py --dir dist/ultra --channel test
"""

from __future__ import annotations

import argparse
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

DEFAULT_HOST = "209.38.55.197"
DEFAULT_PORT = 22
MANIFEST_NAME = "version-manifest.json"
BIN_GLOB = "DSMR-API-V*_8Mb.bin"
FORBIDDEN_ENV = (
    "FTP_PASSWORD",
    "FTP_PASS",
    "FTP_USER",
    "OTA_FTP_USER",
    "OTA_FTP_PASSWORD",
)


@dataclass(frozen=True)
class OtaFiles:
    firmware: Path
    manifest: Path


@dataclass(frozen=True)
class SftpTarget:
    host: str
    user: str
    path: str
    port: int
    identity: Path | None
    password: str | None


def reject_forbidden_env() -> None:
    present = [name for name in FORBIDDEN_ENV if os.environ.get(name)]
    if present:
        raise SystemExit(
            "FTP env vars are not used. Unset "
            + ", ".join(present)
            + " and use GitHub Secrets OTA_SFTP_KEY or OTA_SFTP_PASSWORD."
        )


def find_ota_files(stage_dir: Path) -> OtaFiles:
    if not stage_dir.is_dir():
        raise SystemExit(f"Stage directory not found: {stage_dir}")
    manifest = stage_dir / MANIFEST_NAME
    if not manifest.is_file():
        raise SystemExit(f"Missing {MANIFEST_NAME} in {stage_dir}")
    bins = sorted(
        p
        for p in stage_dir.glob(BIN_GLOB)
        if p.is_file() and "bootloader" not in p.name and "partitions" not in p.name
    )
    if len(bins) != 1:
        names = ", ".join(p.name for p in bins) or "(none)"
        raise SystemExit(f"Need exactly one {BIN_GLOB} in {stage_dir}, found: {names}")
    return OtaFiles(firmware=bins[0], manifest=manifest)


def remote_dir(base_path: str, channel: str, test_path: str = "") -> str:
    if channel == "test" and test_path.strip():
        return test_path.strip().rstrip("/")
    path = base_path.strip()
    if not path:
        raise SystemExit("OTA_SFTP_PATH is required (filesystem dir served as /p1dongle/ultra/)")
    path = path.rstrip("/")
    if channel == "production":
        return path
    if channel == "test":
        return f"{path}/test"
    raise SystemExit(f"channel must be production or test, got {channel!r}")


def load_target(args: argparse.Namespace) -> SftpTarget:
    host = (args.host or os.environ.get("OTA_SFTP_HOST") or DEFAULT_HOST).strip()
    user = (args.user or os.environ.get("OTA_SFTP_USER") or "").strip()
    path = (args.path or os.environ.get("OTA_SFTP_PATH") or "").strip()
    test_path = (getattr(args, "test_path", None) or os.environ.get("OTA_SFTP_PATH_TEST") or "").strip()
    port_raw = args.port if args.port is not None else os.environ.get("OTA_SFTP_PORT")
    identity_raw = args.identity or os.environ.get("OTA_SFTP_IDENTITY") or ""
    password = os.environ.get("OTA_SFTP_PASSWORD") or None
    if not user:
        raise SystemExit("Set OTA_SFTP_USER (GitHub Secret or env). Do not use an FTP account.")
    if not host:
        raise SystemExit("Set OTA_SFTP_HOST (GitHub Secret or env)")
    remote = remote_dir(path, args.channel, test_path)
    port = DEFAULT_PORT
    if port_raw not in (None, ""):
        port = int(port_raw)
    identity = Path(identity_raw).expanduser() if identity_raw else None
    if identity is not None and not identity.is_file():
        raise SystemExit(f"SSH identity not found: {identity}")
    return SftpTarget(
        host=host,
        user=user,
        path=remote,
        port=port,
        identity=identity,
        password=password,
    )


def sftp_batch(files: OtaFiles, remote_path: str) -> str:
    lines = [
        f"put {files.firmware} {remote_path}/{files.firmware.name}",
        f"put {files.manifest} {remote_path}/{files.manifest.name}",
    ]
    return "\n".join(lines) + "\n"


def sftp_command(target: SftpTarget, batch_path: Path) -> list[str]:
    cmd: list[str] = []
    if target.password and target.identity is None:
        cmd.extend(["sshpass", "-e"])
    cmd.extend(
        [
            "sftp",
            "-b",
            str(batch_path),
            "-o",
            f"Port={target.port}",
            "-o",
            "StrictHostKeyChecking=accept-new",
        ]
    )
    if target.identity is not None:
        cmd.extend(
            [
                "-o",
                "BatchMode=yes",
                "-o",
                "IdentitiesOnly=yes",
                "-i",
                str(target.identity),
            ]
        )
    elif target.password:
        cmd.extend(
            [
                "-o",
                "PreferredAuthentications=password",
                "-o",
                "PubkeyAuthentication=no",
            ]
        )
    else:
        cmd.extend(["-o", "BatchMode=yes"])
    cmd.append(f"{target.user}@{target.host}")
    return cmd


def upload(files: OtaFiles, target: SftpTarget, dry_run: bool) -> None:
    batch = sftp_batch(files, target.path)
    if dry_run:
        print(f"dry-run sftp {target.user}@{target.host}:{target.path}")
        print(batch, end="")
        return
    batch_path = files.manifest.with_name(".sftp_ota.batch")
    env = os.environ.copy()
    if target.password and target.identity is None:
        env["SSHPASS"] = target.password
    try:
        batch_path.write_text(batch, encoding="utf-8")
        cmd = sftp_command(target, batch_path)
        printable = [part for part in cmd if part != target.password]
        print("+", " ".join(printable), flush=True)
        subprocess.run(cmd, check=True, env=env)
    finally:
        if batch_path.exists():
            batch_path.unlink()
    print(f"Uploaded {files.firmware.name} then {files.manifest.name} to {target.user}@{target.host}:{target.path}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="SFTP staged Ultra OTA files. Credentials from GitHub Secrets / env, never from git."
    )
    parser.add_argument("--dir", type=Path, required=True, help="Directory with version-manifest.json and *_8Mb.bin")
    parser.add_argument("--channel", choices=("production", "test"), default="production")
    parser.add_argument("--host", help="SSH host (secret/env OTA_SFTP_HOST)")
    parser.add_argument("--user", help="SSH user (secret/env OTA_SFTP_USER)")
    parser.add_argument("--path", help="Remote dir served as /p1dongle/ultra/ (secret/env OTA_SFTP_PATH)")
    parser.add_argument("--test-path", help="Optional test dir (secret/env OTA_SFTP_PATH_TEST)")
    parser.add_argument("--port", type=int, help=f"SSH port (secret/env OTA_SFTP_PORT, default {DEFAULT_PORT})")
    parser.add_argument("--identity", help="Private key file (CI writes secret OTA_SFTP_KEY here)")
    parser.add_argument("--dry-run", action="store_true", help="Print the SFTP plan and do not connect")
    args = parser.parse_args()

    reject_forbidden_env()
    files = find_ota_files(args.dir)
    target = load_target(args)
    upload(files, target, dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
