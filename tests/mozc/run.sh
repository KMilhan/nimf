#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
distro=${1:-ubuntu}
case "$distro" in ubuntu|arch) ;; *) echo 'Usage: bash tests/mozc/run.sh [ubuntu|arch]' >&2; exit 2 ;; esac
mkdir -p build
results=$(mktemp -d "$PWD/build/mozc-$distro-XXXXXX")
echo "Results: $results"

# Some hosts cannot route Docker's bridge network. This affects builds only.
build_network=${BUILD_NETWORK:-default}
if [[ $distro == ubuntu ]]; then
    docker build --network "$build_network" --pull -f .docker/ubuntu.2404.Dockerfile \
        -t nimf-mozc-ubuntu:build . > "$results/base-build.log" 2>&1
fi
pull=()
if [[ $distro == arch ]]; then pull=(--pull); fi
docker build --network "$build_network" "${pull[@]}" -f "tests/mozc/Dockerfile.$distro" \
    -t "nimf-mozc-$distro:test" . > "$results/build.log" 2>&1
docker image inspect "nimf-mozc-$distro:test" > "$results/image.json"
container=$(docker create --init --network none --cap-drop ALL \
    --security-opt no-new-privileges --shm-size 256m "nimf-mozc-$distro:test")
# Called by the EXIT trap, including after interrupts.
# shellcheck disable=SC2329
cleanup() {
    local status=$?
    trap - EXIT
    docker stop --time 5 "$container" >/dev/null || true
    if docker cp "$container:/results/." "$results/"; then
        docker rm "$container" >/dev/null || true
    else
        echo "Could not copy results; container retained: $container" >&2
    fi
    echo "Results: $results"
    exit "$status"
}
trap 'cleanup' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
status=0
timeout --foreground --kill-after=10 180 docker start --attach "$container" > "$results/smoke.log" 2>&1 || status=$?
cat "$results/smoke.log"
exit "$status"
