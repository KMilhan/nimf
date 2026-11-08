#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
distro=${1:-ubuntu}
mode=${2:-sanitize}
case $distro in ubuntu|arch) ;; *) echo 'Expected ubuntu or arch' >&2; exit 2 ;; esac
case $mode in sanitize|analyze) ;; *) echo 'Expected sanitize or analyze' >&2; exit 2 ;; esac
mkdir -p build
results=$(mktemp -d "$PWD/build/memory-$distro-$mode-XXXXXX")
echo "Results: $results"
base="nimf-mozc-$distro:test"
network=${BUILD_NETWORK:-default}
if [[ ${REUSE_BASE:-0} != 1 ]]; then
    if [[ $distro == ubuntu ]]; then
        docker build --network "$network" --pull -f .docker/ubuntu.2404.Dockerfile \
            -t nimf-mozc-ubuntu:build . > "$results/base-build.log" 2>&1
    fi
    pull=()
    if [[ $distro == arch ]]; then pull=(--pull); fi
    docker build --network "$network" "${pull[@]}" -f "tests/mozc/Dockerfile.$distro" \
        -t "$base" . > "$results/toolkits-build.log" 2>&1
fi
docker build --network none -f tests/memory/Dockerfile \
    --build-arg "BASE_IMAGE=$base" --build-arg "CHECK_MODE=$mode" \
    -t "nimf-memory-$distro:$mode" . > "$results/build.log" 2>&1
docker image inspect "nimf-memory-$distro:$mode" > "$results/image.json"
container=$(docker create --init --network none --cap-drop ALL \
    --security-opt no-new-privileges --shm-size 256m "nimf-memory-$distro:$mode")
# shellcheck disable=SC2329
cleanup() {
    local status=$?
    trap - EXIT
    docker stop --timeout 5 "$container" >/dev/null || true
    if docker cp "$container:/results/." "$results/"; then
        docker rm "$container" >/dev/null
    else
        echo "Results unavailable; container retained: $container" >&2
        status=1
    fi
    echo "Results: $results"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
status=0
timeout --foreground --kill-after=10 300 docker start --attach "$container" \
    > "$results/runtime.log" 2>&1 || status=$?
cat "$results/runtime.log"
exit "$status"
