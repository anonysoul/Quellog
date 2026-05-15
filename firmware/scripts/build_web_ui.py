#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path


FIRMWARE_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = FIRMWARE_DIR.parent
WEB_UI_DIR = REPO_ROOT / "device-config-web"
NODE_MODULES_DIR = WEB_UI_DIR / "node_modules"
PACKAGE_JSON = WEB_UI_DIR / "package.json"
PACKAGE_LOCK = WEB_UI_DIR / "package-lock.json"
STAMP_FILE = WEB_UI_DIR / ".npm-ci.stamp"


def require_command(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(
            f"Missing required command: {name}. Install Node.js and npm before building firmware."
        )


def run_command(args: list[str]) -> None:
    subprocess.run(args, cwd=WEB_UI_DIR, check=True)


def read_dependency_fingerprint() -> str:
    digest = hashlib.sha256()
    for path in (PACKAGE_JSON, PACKAGE_LOCK):
        digest.update(path.read_bytes())
    return digest.hexdigest()


def should_run_npm_ci() -> bool:
    if not NODE_MODULES_DIR.is_dir():
        return True
    if not STAMP_FILE.is_file():
        return True
    return STAMP_FILE.read_text(encoding="utf-8").strip() != read_dependency_fingerprint()


def install_dependencies_if_needed() -> None:
    if not PACKAGE_JSON.is_file():
        raise RuntimeError(f"Missing web UI package manifest: {PACKAGE_JSON}")
    if not PACKAGE_LOCK.is_file():
        raise RuntimeError(f"Missing web UI lockfile: {PACKAGE_LOCK}")

    if should_run_npm_ci():
        run_command(["npm", "ci"])
        STAMP_FILE.write_text(read_dependency_fingerprint() + "\n", encoding="utf-8")


def build_web_ui() -> None:
    require_command("node")
    require_command("npm")
    install_dependencies_if_needed()
    run_command(["npm", "run", "build"])


def main() -> int:
    os.chdir(FIRMWARE_DIR)
    build_web_ui()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(1)
