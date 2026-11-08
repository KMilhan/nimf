#!/bin/bash
set -euo pipefail
ulimit -c 0
mkdir -p "$XDG_RUNTIME_DIR" "$HOME/.config"
chmod 700 "$XDG_RUNTIME_DIR"
exec xvfb-run -a -s '-screen 0 1280x800x24 -nolisten tcp' \
    dbus-run-session -- bash /src/tests/mozc/smoke.sh
