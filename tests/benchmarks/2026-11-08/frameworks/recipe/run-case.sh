#!/bin/bash
set -euo pipefail
recipe=$(cd "$(dirname "$0")" && pwd)
cd "$recipe/../../../../.."
distro=$1 framework=$2 language=$3 label=$4
image=${5:-nimf-frameworks-$distro:probe}
result="${COMPARISON_RESULTS:?Set a fresh absolute output directory}/$label-$distro-$framework-$language"
mkdir "$result"
container=
cleanup() {
    status=$?
    trap - EXIT INT TERM
    if [[ -n $container ]]; then
        docker stop --timeout 3 "$container" >/dev/null || true
        if docker cp "$container:/results/." "$result/"; then
            docker rm "$container" >/dev/null
        else
            echo "Results unavailable; retained $container" >&2
            status=1
        fi
    fi
    printf '%s\n' "$status" > "$result/status"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
docker image inspect "$image" > "$result/image.json"
toolkit=
selected_framework=$framework
if [[ $framework == upstream ]]; then selected_framework=nimf; fi
# Ubuntu IBus 1.5.29 fails the Japanese GTK4 commit correctness gate.
if [[ $label == trial* && $distro == ubuntu && $framework == ibus && $language == ja ]]; then toolkit=gtk3; fi
container=$(docker create --init --network none --cap-drop ALL --security-opt no-new-privileges --shm-size 256m \
    -e "IM_FRAMEWORK=$selected_framework" -e "INPUT_LANGUAGE=$language" \
    -e "TOOLKIT=$toolkit" \
    -e "ITERATIONS=${ITERATIONS:-50}" -e "WARMUP=${WARMUP:-10}" -e POLL_US=500 "$image")
printf '%s\n' "$container" > "$result/container.txt"
timeout --foreground --kill-after=10 300 docker start --attach "$container" > "$result/runtime.log" 2>&1 &
wait $!
