#!/bin/bash
# Selects Mozc while mozc_emacs_helper is not on PATH. Keys must reach the
# entry as plain text, and the user must get exactly one notification.
set -euo pipefail
export NIMF_TEST_STATE=/results/missing-helper.state
app_pid=
gsettings set org.nimf setup-environment-variables false
gsettings set org.nimf.engines.nimf-rime active-engine false
gsettings set org.nimf.engines.nimf-mozc active-engine true
gsettings set org.nimf.engines default-engine nimf-mozc

dbus-monitor --session "type='method_call',interface='org.freedesktop.Notifications',member='Notify'" \
    > /results/missing-helper-bus.log 2>&1 &
monitor_pid=$!
empty_path=$(mktemp -d)
env PATH="$empty_path" "$(command -v nimf)" --no-daemon --debug \
    > /results/missing-helper-nimf.log 2>&1 &
nimf_pid=$!
# Called by the EXIT trap.
# shellcheck disable=SC2329
cleanup() {
    local result=$?
    trap - EXIT
    kill "$nimf_pid" "$monitor_pid" ${app_pid:+"$app_pid"} 2>/dev/null || true
    wait 2>/dev/null || true
    exit "$result"
}
trap 'cleanup' EXIT
for ((i=0; i<100; i++)); do
    [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && break
    sleep 0.1
done
test -S "$XDG_RUNTIME_DIR/nimf/socket"

wait_text() {
    local expected=$1
    for ((attempt=0; attempt<100; attempt++)); do
        if [[ -f $NIMF_TEST_STATE ]] && [[ $(head -n1 "$NIMF_TEST_STATE") == "$expected" ]]; then return; fi
        sleep 0.1
    done
    printf 'Expected entry text: %s\nActual:\n' "$expected" >&2
    cat "$NIMF_TEST_STATE" >&2
    return 1
}

nimf-gtk3-probe > /results/missing-helper-app.log 2>&1 &
app_pid=$!
window=$(timeout 15 xdotool search --sync --onlyvisible --pid "$app_pid" --name 'Nimf Mozc probe' | head -n1)
xdotool windowactivate --sync "$window"
xdotool type --clearmodifiers --delay 80 ai
wait_text ai
echo "PASS missing helper keeps plain text input"

# Later keys fall inside the retry delay and must not repeat the message.
xdotool type --clearmodifiers --delay 80 ueo
wait_text aiueo
sleep 0.5
count=$(grep -c 'member=Notify' /results/missing-helper-bus.log || true)
if [[ $count != 1 ]] || ! grep -q 'Mozc is unavailable' /results/missing-helper-bus.log; then
    echo "FAIL missing helper notification: $count Notify calls"
    exit 1
fi
echo "PASS missing helper notifies once"
