#!/bin/bash
set -euo pipefail
cd /src
source tests/memory/flags.sh
bash tests/mozc/build.sh 2>&1 | tee /src/memory-analysis.log
if [[ $NIMF_CHECK_MODE == analyze ]] && grep -q '\[-Wanalyzer-' /src/memory-analysis.log; then
    echo 'FAIL: static analyzer diagnostics' >&2
    exit 1
fi
read -r -a cflags <<< "$CFLAGS"
read -r -a ldflags <<< "$LDFLAGS"
read -r -a deps <<< "$(pkg-config --cflags --libs nimf gio-2.0 gmodule-2.0)"
cc "${cflags[@]}" -Wall -Wextra tests/memory/lifecycle-probe.c \
    -o /usr/local/bin/nimf-lifecycle-probe "${deps[@]}" "${ldflags[@]}"
read -r -a wayland <<< "$(pkg-config --cflags --libs wayland-server)"
cc "${cflags[@]}" -Wall -Wextra -DNIMF_COMPILATION -Ilibnimf tests/memory/wayland-probe.c \
    -o /usr/local/bin/nimf-wayland-memory-probe "${deps[@]}" "${wayland[@]}" "${ldflags[@]}"
