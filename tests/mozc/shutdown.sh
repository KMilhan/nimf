#!/bin/bash
set -euo pipefail
mode=$1
export NIMF_TEST_STATE=/results/shutdown-$mode.state
gsettings set org.nimf setup-environment-variables false
gsettings set org.nimf.engines.nimf-mozc active-engine false
gsettings set org.nimf.engines.nimf-rime active-engine false
gsettings set org.nimf.engines.nimf-anthy active-engine false
gsettings set org.nimf.engines.nimf-libhangul active-engine false
gsettings set org.nimf.engines default-engine nimf-system-keyboard
nimf --no-daemon --debug > "/results/shutdown-$mode.log" 2>&1 &
nimf_pid=$!
app_pid=
# Called by the EXIT trap.
# shellcheck disable=SC2329
cleanup() {
    kill "$nimf_pid" ${app_pid:+"$app_pid"} 2>/dev/null || true
}
trap cleanup EXIT
for ((i=0; i<100; i++)); do
    [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && break
    sleep 0.1
done
test -S "$XDG_RUNTIME_DIR/nimf/socket"
# The socket exists before main() installs its graceful SIGTERM handler.
ready=false
for ((i=0; i<100; i++)); do
    signal_mask=$(awk '/^SigCgt:/ {print $2}' "/proc/$nimf_pid/status")
    if ((16#$signal_mask & (1 << 14))); then
        ready=true
        break
    fi
    sleep 0.1
done
"$ready"
if [[ $mode != no-client ]]; then
    nimf-gtk3-probe > "/results/shutdown-$mode-app.log" 2>&1 &
    app_pid=$!
    window=$(timeout 15 xdotool search --sync --onlyvisible --pid "$app_pid" --name 'Nimf Mozc probe' | head -n1)
    xdotool windowactivate --sync "$window"
    for ((i=0; i<100; i++)); do
        if [[ -f $NIMF_TEST_STATE ]] && [[ $(sed -n '3p' "$NIMF_TEST_STATE") == 1 ]]; then break; fi
        sleep 0.1
    done
    test "$(sed -n '3p' "$NIMF_TEST_STATE")" = 1
    if [[ $mode == client-first ]]; then
        xdotool key alt+F4
        wait "$app_pid"
        app_pid=
    fi
fi
kill "$nimf_pid"
for ((i=0; i<30; i++)); do
    kill -0 "$nimf_pid" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$nimf_pid" 2>/dev/null; then
    kill -KILL "$nimf_pid"
    echo "FAIL shutdown $mode: server did not exit" >&2
    exit 1
fi
wait "$nimf_pid"
echo "PASS shutdown $mode without Mozc"
