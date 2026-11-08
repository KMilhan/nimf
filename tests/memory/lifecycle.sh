#!/bin/bash
set -euo pipefail
gsettings set org.nimf setup-environment-variables false
gsettings set org.nimf.engines.nimf-rime active-engine false
gsettings set org.nimf.engines.nimf-anthy active-engine false
gsettings set org.nimf.engines.nimf-libhangul active-engine false
gsettings set org.nimf.engines.nimf-mozc active-engine true
gsettings set org.nimf.engines default-engine nimf-mozc
nimf-wayland-memory-probe "$(pkg-config --variable=libdir nimf)/nimf/modules/services/libnimf-wayland.so" \
    > /results/wayland-lifecycle.log 2>&1
cat /results/wayland-lifecycle.log
server=
# shellcheck disable=SC2329
cleanup() { if [[ -n $server ]]; then kill "$server" 2>/dev/null || true; fi; }
trap cleanup EXIT
start_server() {
    nimf --no-daemon --debug > "$1" 2>&1 &
    server=$!
    for ((i=0; i<100; i++)); do
        signal_mask=$(awk '/^SigCgt:/ {print $2}' "/proc/$server/status")
        if [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && ((16#$signal_mask & (1 << 14))); then
            return
        fi
        sleep 0.1
    done
    echo 'FAIL server readiness' >&2
    return 1
}
start_server /results/lifecycle-server.log
for mode in methods repeat preedit-toggle threads reentrant disconnect unload; do
    timeout 30 nimf-lifecycle-probe "$mode" "$(pkg-config --variable=libdir nimf)/nimf/modules" \
        > "/results/lifecycle-$mode.log" 2>&1
    cat "/results/lifecycle-$mode.log"
    if [[ $mode == disconnect ]]; then echo 'PASS lifecycle abrupt disconnect'; fi
done
openbox > /results/settings-window-manager.log 2>&1 &
nimf-settings > /results/settings.log 2>&1 &
settings=$!
window=$(timeout 15 xdotool search --sync --onlyvisible --pid "$settings" | head -n1)
xdotool windowactivate --sync "$window"
import -window root /results/settings.png
xdotool key Tab Tab shift+Tab alt+F4
for ((i=0; i<50; i++)); do
    kill -0 "$settings" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$settings" 2>/dev/null; then
    kill "$settings"
    echo 'FAIL settings close' >&2
    exit 1
fi
wait "$settings"
echo 'PASS settings open, keyboard focus, and close'
kill "$server"
for ((i=0; i<50; i++)); do
    kill -0 "$server" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$server" 2>/dev/null; then
    kill -KILL "$server"
    echo 'FAIL lifecycle server shutdown' >&2
    exit 1
fi
wait "$server"
echo 'PASS lifecycle server shutdown'
gsettings set org.nimf.engines.nimf-mozc active-engine true
gsettings set org.nimf.engines default-engine nimf-mozc
start_server /results/lifecycle-interrupted-server.log
NIMF_TEST_SERVER_PID=$server timeout 30 nimf-lifecycle-probe server-disconnect \
    > /results/lifecycle-server-disconnect.log 2>&1
cat /results/lifecycle-server-disconnect.log
status=0
wait "$server" || status=$?
test "$status" = 137
trap - EXIT
