#!/usr/bin/env bash
# Isolated application dependencies only; no driver/kernel/security changes.
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-python3}"
"$PYTHON" -c 'import sys; assert sys.version_info >= (3,11), "Use Python 3.11+; macOS system Python may be older"'
[[ -d .venv ]] || "$PYTHON" -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m serial.tools.list_ports -v
if [[ -f server.py && -f scripts/start_macos.sh ]]; then
  printf '\nNext: bash scripts/start_macos.sh\n'
else
  printf '\nCLI ready: .venv/bin/python src/firmware/tools/imu_probe.py --port YOUR_PORT diag\n'
  printf 'The local 3D server is in the complete companion download; see README Delivery scope.\n'
fi
