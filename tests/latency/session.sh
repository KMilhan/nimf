#!/bin/bash
set -euo pipefail
ulimit -c 0
{
    cat /etc/os-release
    cc --version | head -n1
    pkg-config --modversion glib-2.0 gtk+-3.0 gtk4 x11 xtst
    pkg-config --modversion libhangul
    grep -E '^(CFLAGS|CXXFLAGS|LDFLAGS) =' /src/Makefile
    if command -v dpkg-query >/dev/null; then
        dpkg-query -W emacs-mozc-bin mozc-server xvfb
    else
        pacman -Q xorg-server-xvfb
        printf 'Mozc source revision: '
        cat /opt/mozc/.git/HEAD
    fi
    if command -v ibus >/dev/null; then ibus version; fi
    if command -v fcitx5 >/dev/null; then fcitx5 --version; fi
    sha256sum /usr/bin/mozc_emacs_helper /usr/lib/mozc/mozc_server
    printf 'iterations=%s warmup=%s poll_us=%s\n' "${ITERATIONS:-100}" "${WARMUP:-10}" "${POLL_US:-500}"
    printf 'framework=%s language=%s\n' "${IM_FRAMEWORK:-nimf}" "${INPUT_LANGUAGE:-ja}"
    uname -a
    sed -n '/^model name/ {p;q;}' /proc/cpuinfo
} > /results/versions.txt
if ldd /usr/bin/nimf | grep -Eq 'libasan|libubsan'; then
    echo 'Latency measurements require a build without sanitizers' >&2
    exit 1
fi
export LANG=C.UTF-8 GDK_BACKEND=x11 NO_AT_BRIDGE=1
export GSK_RENDERER=cairo GTK_A11Y=none
toolkits=(gtk3 gtk4)
if [[ -n ${TOOLKIT:-} ]]; then
    case "$TOOLKIT" in gtk3|gtk4) toolkits=("$TOOLKIT") ;; *) echo 'TOOLKIT must be gtk3 or gtk4' >&2; exit 2 ;; esac
fi
for toolkit in "${toolkits[@]}"; do
    unset GDK_DISABLE GDK_DEBUG
    if [[ $toolkit == gtk4 ]]; then
        export GDK_DISABLE=gl,vulkan
        if ! pkg-config --atleast-version=4.18 gtk4; then
            export GDK_DEBUG=gl-disable,vulkan-disable
        fi
    fi
    profile=$(mktemp -d /tmp/nimf-latency-XXXXXX)
    mkdir -m 700 "$profile/config" "$profile/runtime"
    mkdir -p "/results/$toolkit"
    env HOME="$profile" XDG_CONFIG_HOME="$profile/config" XDG_RUNTIME_DIR="$profile/runtime" \
        xvfb-run -a -s '-screen 0 1024x768x24 -nolisten tcp' \
        dbus-run-session -- bash /src/tests/latency/benchmark.sh "$toolkit"
done
