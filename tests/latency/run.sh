#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
distro=${1:-ubuntu}
case "$distro" in ubuntu|arch) ;; *) echo 'Usage: bash tests/latency/run.sh [ubuntu|arch]' >&2; exit 2 ;; esac
mkdir -p build
results=$(mktemp -d "$PWD/build/latency-$distro-XXXXXX")
echo "Results: $results"
base="nimf-mozc-$distro:test"
network=${BUILD_NETWORK:-default}
container=
command_pid=
# Run long commands through wait so signals can interrupt the shell promptly.
run_command() {
    local status=0
    "$@" &
    command_pid=$!
    wait "$command_pid" || status=$?
    command_pid=
    return "$status"
}
# shellcheck disable=SC2329
cleanup() {
    local status=$?
    trap - EXIT INT TERM
    if [[ -n $container ]]; then
        docker stop --timeout 5 "$container" >/dev/null || true
    fi
    if [[ -n $command_pid ]]; then
        kill "$command_pid" 2>/dev/null || true
        wait "$command_pid" 2>/dev/null || true
    fi
    if [[ -n $container ]]; then
        if docker cp "$container:/results/." "$results/"; then
            docker rm "$container" >/dev/null
        else
            echo "Results unavailable; container retained: $container" >&2
            status=1
        fi
    fi
    echo "Results: $results"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
git rev-parse HEAD > "$results/revision.txt"
git status --porcelain=v1 > "$results/worktree.txt"
git diff --binary HEAD > "$results/source.patch"
git ls-files --cached --others --exclude-standard -z |
    while IFS= read -r -d '' source_file; do
        if [[ -f $source_file ]]; then printf '%s\0' "$source_file"; fi
    done | sort -z | xargs -0 sha256sum > "$results/source.sha256"
if [[ ${REUSE_BASE:-0} != 1 ]]; then
    if [[ $distro == ubuntu ]]; then
        run_command docker build --network "$network" --pull -f .docker/ubuntu.2404.Dockerfile \
            -t nimf-mozc-ubuntu:build . > "$results/base-build.log" 2>&1
    fi
    pull=()
    if [[ $distro == arch ]]; then pull=(--pull); fi
    run_command docker build --network "$network" "${pull[@]}" -f "tests/mozc/Dockerfile.$distro" \
        -t "$base" . > "$results/toolkits-build.log" 2>&1
fi
run_command docker build --network none -f tests/latency/Dockerfile --build-arg "BASE_IMAGE=$base" \
    -t "nimf-latency-$distro:test" . > "$results/build.log" 2>&1
docker image inspect "nimf-latency-$distro:test" > "$results/image.json"
container=$(docker create --init --network none --cap-drop ALL \
    --security-opt no-new-privileges --shm-size 256m \
    -e "ITERATIONS=${ITERATIONS:-100}" -e "WARMUP=${WARMUP:-10}" \
    -e "INPUT_LANGUAGE=${INPUT_LANGUAGE:-ja}" \
    -e "TOOLKIT=${TOOLKIT:-}" \
    -e "POLL_US=${POLL_US:-500}" "nimf-latency-$distro:test")
printf '%s\n' "$container" > "$results/container.txt"
status=0
run_command timeout --foreground --kill-after=10 "${RUN_TIMEOUT:-600}" \
    docker start --attach "$container" > "$results/runtime.log" 2>&1 || status=$?
cat "$results/runtime.log"
exit "$status"
