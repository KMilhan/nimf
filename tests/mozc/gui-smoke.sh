#!/bin/bash
set -euo pipefail
toolkit=$1
if [[ $toolkit == gtk4 && ${NIMF_CHECK_MODE:-} == sanitize ]]; then
    # The Xvfb test uses Cairo; do not initialize unused GPU driver caches.
    export GDK_DISABLE=gl,vulkan GTK_A11Y=none
    if ! pkg-config --atleast-version=4.18 gtk4; then
        export GDK_DEBUG=gl-disable,vulkan-disable
    fi
fi
export NIMF_TEST_STATE=/results/$toolkit.state
status=0
app_pid=
gsettings set org.nimf setup-environment-variables false
# Rime's first-run dictionary maintenance is unrelated to the Mozc scenarios.
gsettings set org.nimf.engines.nimf-rime active-engine false
gsettings set org.nimf.engines.nimf-mozc active-engine true
gsettings set org.nimf.engines default-engine nimf-mozc
nimf --no-daemon --debug > "/results/$toolkit-nimf.log" 2>&1 &
nimf_pid=$!
# Called by the EXIT trap.
# shellcheck disable=SC2329
cleanup() {
    local result=$? app_result=0 server_result=0
    trap - EXIT
    kill "$nimf_pid" 2>/dev/null || true
    # Keep the client connected until the server has released its contexts.
    for ((i=0; i<20; i++)); do
        if ! kill -0 "$nimf_pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    kill -KILL "$nimf_pid" 2>/dev/null || true
    if [[ -n $app_pid ]]; then
        if kill -0 "$app_pid" 2>/dev/null && [[ -n ${window:-} ]]; then
            xdotool windowactivate --sync "$window" key alt+F4 || true
        fi
        for ((i=0; i<20; i++)); do
            kill -0 "$app_pid" 2>/dev/null || break
            sleep 0.1
        done
        kill -KILL "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || app_result=$?
    fi
    wait "$nimf_pid" 2>/dev/null || server_result=$?
    if ((server_result != 0 || app_result != 0)); then
        echo "FAIL $toolkit shutdown: server=$server_result app=$app_result"
        result=1
    else
        echo "PASS $toolkit server shutdown with connected client"
    fi
    exit "$result"
}
trap 'cleanup' EXIT
for ((i=0; i<100; i++)); do
    [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && break
    sleep 0.1
done
test -S "$XDG_RUNTIME_DIR/nimf/socket"

wait_text() {
    local expected=$1 line=${2:-1}
    for ((attempt=0; attempt<100; attempt++)); do
        if [[ -f $NIMF_TEST_STATE ]] && [[ $(sed -n "${line}p" "$NIMF_TEST_STATE") == "$expected" ]]; then return; fi
        sleep 0.1
    done
    printf 'Expected entry/state line %s: %s\nActual:\n' "$line" "$expected" >&2
    cat "$NIMF_TEST_STATE" >&2
    return 1
}

wait_other_text() {
    local previous=$1 current
    for ((attempt=0; attempt<100; attempt++)); do
        current=$(head -n1 "$NIMF_TEST_STATE")
        if [[ -n $current && $current != "$previous" ]]; then return; fi
        sleep 0.1
    done
    echo "Expected a nonempty commit different from: $previous" >&2
    cat "$NIMF_TEST_STATE" >&2
    return 1
}

clear_entry() {
    xdotool key ctrl+a BackSpace
    wait_text ''
}

check_candidate_position() {
    local candidate app_x app_y app_width app_height popup_x popup_y
    if ! candidate=$(timeout 3 xdotool search --sync --onlyvisible --pid "$nimf_pid" | head -n1); then
        echo "FAIL $toolkit candidate window missing"
        return 1
    fi
    read -r app_x app_y app_width app_height < <(xdotool getwindowgeometry --shell "$window" | sed -n 's/^[XY]=//p; s/^WIDTH=//p; s/^HEIGHT=//p' | xargs) || return 1
    read -r popup_x popup_y < <(xdotool getwindowgeometry --shell "$candidate" | sed -n 's/^[XY]=//p' | xargs) || return 1
    if ((popup_x < app_x || popup_y < app_y ||
         popup_x > app_x + app_width || popup_y > app_y + app_height + 100)); then
        echo "FAIL $toolkit candidate position: popup=$popup_x,$popup_y app=$app_x,$app_y"
        return 1
    fi
    echo "PASS $toolkit candidate position: popup=$popup_x,$popup_y app=$app_x,$app_y"
}

"nimf-$toolkit-probe" > "/results/$toolkit.log" 2>&1 &
app_pid=$!
window=$(timeout 15 xdotool search --sync --onlyvisible --pid "$app_pid" --name 'Nimf Mozc probe' | head -n1)
xdotool windowmove --sync "$window" 400 300
xdotool windowactivate --sync "$window"
wait_text 1 3
xdotool type --clearmodifiers --delay 80 nihongo
sleep 0.3
wait_text ''
import -window root "/results/$toolkit-preedit.png"
xdotool key space Return
wait_text 日本語
import -window root "/results/$toolkit-committed.png"
echo "PASS $toolkit conversion"

clear_entry
xdotool type --clearmodifiers --delay 80 ai
xdotool key BackSpace Return
wait_text あ
echo "PASS $toolkit backspace"

clear_entry
xdotool type --clearmodifiers --delay 80 kana
xdotool key Escape Escape
xdotool type --clearmodifiers a
xdotool key Return
wait_text あ
echo "PASS $toolkit cancellation"

clear_entry
xdotool type --clearmodifiers --delay 80 kana
xdotool windowminimize "$window"
wait_text かな
xdotool windowmap "$window"
xdotool windowactivate --sync "$window"
xdotool key Tab
wait_text 2 3
xdotool type --clearmodifiers --delay 80 ai
xdotool key Return
wait_text あい 2
echo "PASS $toolkit focus transfer"

xdotool key shift+Tab
wait_text 1 3
clear_entry
old_helper=$(pgrep -P "$nimf_pid" -f mozc_emacs_helper)
kill "$old_helper"
sleep 0.2
xdotool type --clearmodifiers x
sleep 0.2
clear_entry
xdotool type --clearmodifiers --delay 80 nihongo
xdotool key space Return
wait_text 日本語
new_helper=$(pgrep -P "$nimf_pid" -f mozc_emacs_helper)
[[ $old_helper != "$new_helper" ]]
echo "PASS $toolkit helper recovery"

clear_entry
xdotool type --clearmodifiers --delay 80 nihongo
xdotool key space space Up
candidate=
if candidate=$(timeout 3 xdotool search --sync --onlyvisible --pid "$nimf_pid" | head -n1); then
    check_candidate_position || status=1
else
    echo "FAIL $toolkit candidate window missing"
    status=1
fi
import -window root "/results/$toolkit-candidates.png"
if [[ -n $candidate ]]; then
    xdotool mousemove --window "$candidate" 60 48 click 1
    sleep 0.2
    xdotool key Return
    wait_text ニホンゴ
    echo "PASS $toolkit candidate mouse selection"
else
    echo "SKIP $toolkit candidate mouse selection: no window"
    xdotool key Return
    wait_text 日本語
fi

# Capture the current first candidate because Mozc learns previous selections.
clear_entry
xdotool type --clearmodifiers --delay 80 nihongo
xdotool key space space Up Return
wait_other_text ''
first_candidate=$(head -n1 "$NIMF_TEST_STATE")
clear_entry
xdotool type --clearmodifiers --delay 80 nihongo
xdotool key space space Up Next
sleep 0.3
import -window root "/results/$toolkit-next-page.png"
xdotool key Return
wait_other_text "$first_candidate"
echo "PASS $toolkit next-page commit"

# Reopen at another window position and in the second entry.
xdotool windowmove --sync "$window" 150 100
xdotool key Tab
wait_text 2 3
xdotool key ctrl+a BackSpace
wait_text '' 2
xdotool type --clearmodifiers --delay 80 nihongo
xdotool key space space Up
check_candidate_position || status=1
import -window root "/results/$toolkit-moved-candidates.png"
xdotool key Escape Escape
exit "$status"
