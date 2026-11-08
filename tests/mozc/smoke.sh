#!/bin/bash
set -euo pipefail
export GTK_IM_MODULE=nimf QT_IM_MODULE=nimf XMODIFIERS=@im=nimf
export GDK_BACKEND=x11 QT_QPA_PLATFORM=xcb GSK_RENDERER=cairo

{
    cat /etc/os-release
    pkg-config --modversion gtk+-3.0 gtk4 Qt5Core Qt6Core
    cc --version
    mozc_emacs_helper < /dev/null
    if command -v pacman >/dev/null; then pacman -Q; else dpkg-query -W; fi
} > /results/versions.txt
cp /src/modules/engines/nimf-mozc/test-nimf-mozc.log /results/unit.log
status=0
if nimf-helper-probe; then
    echo 'PASS engine protocol'
else
    status=1
fi
openbox > /results/openbox.log 2>&1 &
for mode in no-client client-first server-first; do
    profile=$(mktemp -d "/tmp/shutdown-$mode-XXXXXX")
    mkdir -m 700 "$profile/config" "$profile/runtime"
    if ! env XDG_CONFIG_HOME="$profile/config" XDG_RUNTIME_DIR="$profile/runtime" \
        dbus-run-session -- bash /src/tests/mozc/shutdown.sh "$mode"; then
        echo "FAIL shutdown $mode"
        status=1
    fi
done
for toolkit in gtk3 gtk4 qt5 qt6; do
    profile=$(mktemp -d "/tmp/mozc-$toolkit-XXXXXX")
    mkdir -m 700 "$profile/config" "$profile/runtime"
    if env NIMF_TEST_CSD=1 XDG_CONFIG_HOME="$profile/config" XDG_RUNTIME_DIR="$profile/runtime" \
        dbus-run-session -- bash /src/tests/mozc/gui-smoke.sh "$toolkit"; then
        echo "PASS $toolkit GUI"
    else
        echo "FAIL $toolkit GUI"
        status=1
    fi
done
exit "$status"
