#!/bin/bash
# Runs inside a benchmark image. The GTK3 and GTK4 Nimf modules must report a
# fresh cursor location when the application window moves, even if the
# application reports the same area. The module log shows how many reports each
# toolkit sent.
set -euo pipefail
export LANG=C.UTF-8 GDK_BACKEND=x11 NO_AT_BRIDGE=1 GTK_A11Y=none
result=/results
mkdir -p "$result"
export XDG_RUNTIME_DIR=$(mktemp -d /tmp/nimf-cursor-runtime-XXXXXX)
export XDG_CONFIG_HOME=$(mktemp -d /tmp/nimf-cursor-config-XXXXXX)
chmod 700 "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"

run_inner ()
{
    export GTK_IM_MODULE=nimf
    gsettings set org.nimf setup-environment-variables false
    for candidate in rime anthy libhangul mozc; do
        active=false
        if [[ $candidate == libhangul ]]; then active=true; fi
        gsettings set "org.nimf.engines.nimf-$candidate" active-engine "$active"
    done
    gsettings set org.nimf.engines default-engine nimf-libhangul

    nimf --no-daemon > "$result/nimf.log" 2>&1 &
    daemon=$!
    for ((attempt = 0; attempt < 100; attempt++)); do
        [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && break
        sleep 0.1
    done
    if ((attempt == 100)); then
        echo "Nimf did not become ready" >&2
        kill "$daemon" 2>/dev/null || true
        exit 1
    fi

    for toolkit in gtk3 gtk4; do
        G_MESSAGES_DEBUG=all "/tmp/cursor-move-$toolkit" > "$result/$toolkit-app.log" 2>&1
        sleep 1
    done
    kill "$daemon" 2>/dev/null || true
}

if [[ ${1:-} == --inner ]]; then
    run_inner
    exit 0
fi

Xvfb :99 -screen 0 800x600x24 > "$result/xvfb.log" 2>&1 &
server=$!
# shellcheck disable=SC2329
cleanup () { kill "$server" 2>/dev/null || true; }
trap cleanup EXIT
export DISPLAY=:99

for toolkit in gtk3 gtk4; do
    if [[ $toolkit == gtk3 ]]; then flags=$(pkg-config --cflags --libs gtk+-3.0); else flags=$(pkg-config --cflags --libs gtk4 x11); fi
    if ! gcc -Wall -Wextra -Werror -Wno-error=deprecated-declarations -o "/tmp/cursor-move-$toolkit" "/tmp/cursor-move-$toolkit.c" $flags > "$result/build-$toolkit.log" 2>&1; then
        echo "Build failed for $toolkit" >&2
        cat "$result/build-$toolkit.log" >&2
        exit 1
    fi
done

dbus-run-session -- bash /tmp/session.sh --inner

# Each program reports the same area twice before and twice after the move.
# One report before the move and one after it is the correct result.
status=0
for toolkit in gtk3 gtk4; do
    count=$(grep -c "send: NIMF_MESSAGE_SET_CURSOR_LOCATION" "$result/$toolkit-app.log" || true)
    printf "%s_set_cursor_location_messages=%s\n" "$toolkit" "$count" >> "$result/summary.txt"
    grep "NIMF_MESSAGE_SET_CURSOR_LOCATION" "$result/$toolkit-app.log" >> "$result/summary.txt" || true
    if [[ $count == 2 ]]; then
        echo "PASS: $toolkit reports a fresh cursor location after a move"
    else
        echo "FAIL: $toolkit expected 2 cursor location reports, got $count" >&2
        status=1
    fi
done
exit "$status"
