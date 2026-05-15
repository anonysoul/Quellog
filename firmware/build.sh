#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

load_idf_env() {
  if command -v idf.py >/dev/null 2>&1; then
    return 0
  fi

  if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
    # shellcheck disable=SC1090
    source "${IDF_PATH}/export.sh"
  fi

  if command -v idf.py >/dev/null 2>&1; then
    return 0
  fi

  for export_sh in "$HOME/esp/esp-idf/export.sh" "/root/esp/esp-idf/export.sh"; do
    if [[ -f "$export_sh" ]]; then
      # shellcheck disable=SC1090
      source "$export_sh"
      break
    fi
  done

  if ! command -v idf.py >/dev/null 2>&1; then
    echo "[ERROR] ESP-IDF environment not found. Run source export.sh first." >&2
    exit 1
  fi
}

cd "$PROJECT_DIR"
load_idf_env

idf.py set-target esp32s3
idf.py build
