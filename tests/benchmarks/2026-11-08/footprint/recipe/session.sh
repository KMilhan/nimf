#!/bin/bash
set -euo pipefail
ulimit -c 0
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
read -r -a flags <<< "$(pkg-config --cflags --libs nimf)"
cc -g -O2 -Wall -Wextra /probe/probe.c -o /tmp/footprint-probe "${flags[@]}"
exec xvfb-run -a -s '-screen 0 1280x800x24 -nolisten tcp' \
  dbus-run-session -- python3 /probe/measure.py
