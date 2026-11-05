#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# Always rebuild safe target. Never use the historical tracked build/sms_app.
make offline
exec python3 tests/verify_offline.py build/sms_app_offline
