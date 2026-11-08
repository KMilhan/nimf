#!/bin/bash
set -euo pipefail
toolkit=$1
result="/results/$toolkit"
# shellcheck source=tests/latency/framework.sh
source /src/tests/latency/framework.sh "$result"
# shellcheck disable=SC2329
cleanup() { kill "$server" 2>/dev/null || true; }
trap cleanup EXIT
for ((attempt=0; attempt<100; attempt++)); do
    kill -0 "$server"
    case "$framework" in
        nimf) [[ -S $XDG_RUNTIME_DIR/nimf/socket ]] && break ;;
        ibus) ibus engine "$engine" 2>/dev/null && break ;;
        fcitx5)
            # Query the bus itself to avoid activating a competing daemon.
            owner=$(gdbus call --session --dest org.freedesktop.DBus \
                --object-path /org/freedesktop/DBus \
                --method org.freedesktop.DBus.NameHasOwner org.fcitx.Fcitx5)
            [[ $owner == '(true,)' ]] && break
            ;;
    esac
    sleep 0.1
done
kill -0 "$server"
if ((attempt == 100)); then echo 'Input framework did not become ready' >&2; exit 1; fi
# The indicator creates its status icon three seconds after startup.
sleep 4
nimf-latency-driver --probe "/usr/local/bin/nimf-latency-$toolkit" \
    --language "$language" \
    --output "/results/$toolkit" --iterations "${ITERATIONS:-100}" \
    --warmup "${WARMUP:-10}" --poll-us "${POLL_US:-500}" \
    > "/results/$toolkit/summary.txt" 2> "/results/$toolkit/probe.log"
cat "/results/$toolkit/summary.txt"
for raster in "$result"/*.ppm; do
    convert "$raster" "${raster%.ppm}.png"
done
first_step=preedit
preedit=あ
ascii=a
if [[ $language == ko ]]; then first_step='preedit-initial'; preedit=ㅎ; ascii=g; fi
# An invisible entry still processes input. It must not pass the screen endpoint.
if NIMF_LATENCY_HIDE_ENTRY=1 nimf-latency-driver \
    --language "$language" \
    --probe "/usr/local/bin/nimf-latency-$toolkit" --output "/results/$toolkit/negative" \
    --iterations 1 --warmup 0 --timeout-ms 500 \
    > "/results/$toolkit/negative.log" 2>&1; then
    echo 'FAIL: invisible input passed screen detection' >&2
    exit 1
fi
grep -Fq "Timeout at $first_step:" "/results/$toolkit/negative.log"
grep -Fq "preedit='$preedit' text=''" "/results/$toolkit/negative.log"
grep -Fq 'final_pixel_change=0' "/results/$toolkit/negative.log"
echo 'PASS: semantic input without pixels is rejected'
# Visible ASCII from a different input context must not count as engine output.
if GTK_IM_MODULE=gtk-im-context-simple nimf-latency-driver \
    --language "$language" \
    --probe "/usr/local/bin/nimf-latency-$toolkit" --output "/results/$toolkit/wrong-context" \
    --iterations 1 --warmup 0 --timeout-ms 500 \
    > "/results/$toolkit/wrong-context.log" 2>&1; then
    echo 'FAIL: incorrect text passed semantic validation' >&2
    exit 1
fi
grep -Fq "Timeout at $first_step:" "/results/$toolkit/wrong-context.log"
grep -Fq "preedit='' text='$ascii'" "/results/$toolkit/wrong-context.log"
grep -Fq 'final_pixel_change=1' "/results/$toolkit/wrong-context.log"
echo 'PASS: visible output from the wrong input context is rejected'
kill "$server"
shutdown=graceful
for ((attempt=0; attempt<50; attempt++)); do
    if ! kill -0 "$server" 2>/dev/null; then break; fi
    sleep 0.1
done
if kill -0 "$server" 2>/dev/null; then
    shutdown=forced
    echo 'WARNING: daemon did not exit within five seconds; forcing shutdown' >&2
    kill -KILL "$server"
fi
printf '%s\n' "$shutdown" > "$result/shutdown.txt"
status=0
wait "$server" || status=$?
if [[ $status != 0 && $status != 143 && ! ( $shutdown == forced && $status == 137 ) ]]; then
    exit "$status"
fi
trap - EXIT
