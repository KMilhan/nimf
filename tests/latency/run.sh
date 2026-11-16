#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
distro=${1:-ubuntu}
case "$distro" in ubuntu|arch) ;; *) echo 'Usage: bash tests/latency/run.sh [ubuntu|arch]' >&2; exit 2 ;; esac
framework=${IM_FRAMEWORK:-nimf}
case "$framework" in nimf|ibus|fcitx5|upstream) ;; *) echo 'IM_FRAMEWORK must be nimf, ibus, fcitx5, or upstream' >&2; exit 2 ;; esac
language=${INPUT_LANGUAGE:-ja}
case "$language" in ja|ko) ;; *) echo 'INPUT_LANGUAGE must be ja or ko' >&2; exit 2 ;; esac
if [[ $framework == upstream && $language == ja ]]; then
    echo 'Upstream Nimf has no Mozc engine; set INPUT_LANGUAGE=ko or use IM_FRAMEWORK=nimf' >&2
    exit 2
fi
profile=${PROFILE:-0}
case "$profile" in 0|1) ;; *) echo 'PROFILE must be 0 or 1' >&2; exit 2 ;; esac
if [[ $profile == 1 && $framework != nimf ]]; then
    echo 'Profiling supports nimf only; unset PROFILE or set IM_FRAMEWORK=nimf' >&2
    exit 2
fi
mkdir -p build
results=$(mktemp -d "$PWD/build/latency-$distro-XXXXXX")
echo "Results: $results"
base="nimf-mozc-$distro:test"
nimf_image="nimf-latency-$distro:test"
if [[ $profile == 1 ]]; then nimf_image="nimf-latency-$distro:profile"; fi
image="$nimf_image"
if [[ $framework == upstream ]]; then image="nimf-latency-$distro:upstream"; fi
if [[ $framework == ibus || $framework == fcitx5 ]]; then image="nimf-latency-$distro:frameworks-$framework"; fi
upstream_revision=${UPSTREAM_REVISION:-b4a141aec1c833541f163da8b1beb9ab1e0c0615}
network=${BUILD_NETWORK:-default}
cpuset=${CPUSET:-}
case "$cpuset" in
    *[!0-9,-]*) echo "CPUSET must list CPU numbers, for example 0-3 or 2,5" >&2; exit 2 ;;
esac
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
    --build-arg "NIMF_BUILD_PROFILING=$profile" \
    -t "$nimf_image" . > "$results/build.log" 2>&1
if [[ $framework == upstream ]]; then
    run_command docker build --network "$network" -f tests/latency/Dockerfile.upstream \
        --build-arg "BASE_IMAGE=$nimf_image" --build-arg "UPSTREAM_REVISION=$upstream_revision" \
        -t "$image" . > "$results/upstream-build.log" 2>&1
elif [[ $framework != nimf ]]; then
    run_command docker build --network "$network" \
        -f "tests/latency/Dockerfile.frameworks.$distro" --build-arg "BASE_IMAGE=$nimf_image" \
        --build-arg "IM_FRAMEWORK=$framework" --build-arg "INPUT_LANGUAGE=$language" \
        -t "$image" . > "$results/frameworks-build.log" 2>&1
fi
docker image inspect "$image" > "$results/image.json"
cpuset_args=()
if [[ -n $cpuset ]]; then cpuset_args=(--cpuset-cpus "$cpuset"); fi
printf "cpuset=%s\n" "${CPUSET:-none}" > "$results/cpuset.txt"
container=$(docker create --init --network none --cap-drop ALL \
    --security-opt no-new-privileges --shm-size 256m \
    ${cpuset_args[@]+"${cpuset_args[@]}"} \
    -e "ITERATIONS=${ITERATIONS:-100}" -e "WARMUP=${WARMUP:-10}" \
    -e "INPUT_LANGUAGE=$language" \
    -e "IM_FRAMEWORK=$framework" \
    -e "TOOLKIT=${TOOLKIT:-}" \
    -e "PROFILE=$profile" -e "NIMF_PROFILE_CPU=${PROFILE_CPU:-0}" \
    -e "NIMF_PROFILE_CAPACITY=${PROFILE_CAPACITY:-65536}" \
    -e "POLL_US=${POLL_US:-500}" "$image")
printf '%s\n' "$container" > "$results/container.txt"
status=0
run_command timeout --foreground --kill-after=10 "${RUN_TIMEOUT:-600}" \
    docker start --attach "$container" > "$results/runtime.log" 2>&1 || status=$?
cat "$results/runtime.log"
exit "$status"
