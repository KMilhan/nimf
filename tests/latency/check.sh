#!/bin/bash
# Runner contract checks with a fake docker binary. No container starts.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
output=$(mktemp -d "$PWD/build/latency-check-XXXXXX")
bin="$output/bin"
mkdir -p "$bin"
printf "%s\n" \
    "#!/bin/bash" \
    "echo \"\$*\" >> \"\$FAKE_DOCKER_LOG\"" \
    "if [ \"\$1\" = create ]; then echo fake-container; fi" \
    "exit 0" > "$bin/docker"
chmod +x "$bin/docker"
status=0
run_runner() {
    local name=$1 distro=$2
    shift 2
    : > "$output/$name.calls"
    env "$@" FAKE_DOCKER_LOG="$output/$name.calls" PATH="$bin:$PATH" REUSE_BASE=1 \
        ITERATIONS=1 WARMUP=0 RUN_TIMEOUT=10 \
        timeout 60 bash tests/latency/run.sh "$distro" \
        > "$output/$name.out" 2> "$output/$name.err"
}
check() {
    local expected=$1 name=$2 distro=$3
    shift 3
    local actual=0
    run_runner "$name" "$distro" "$@" || actual=$?
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: $name expected exit $expected, got $actual" >&2
        sed -n "1,10p" "$output/$name.err" >&2
        status=1
    fi
}
assert_call() {
    local name=$1 pattern=$2
    if ! grep -Fq -- "$pattern" "$output/$name.calls"; then
        echo "FAIL: $name misses docker call: $pattern" >&2
        status=1
    fi
}
assert_no_call() {
    local name=$1 pattern=$2
    if grep -Fq -- "$pattern" "$output/$name.calls"; then
        echo "FAIL: $name has unexpected docker call: $pattern" >&2
        status=1
    fi
}
assert_no_docker() {
    local name=$1
    if [ -s "$output/$name.calls" ]; then
        echo "FAIL: $name reached docker before validation" >&2
        status=1
    fi
}
assert_stderr() {
    local name=$1 pattern=$2
    if ! grep -Fq -- "$pattern" "$output/$name.err"; then
        echo "FAIL: $name misses message: $pattern" >&2
        status=1
    fi
}
check 0 nimf ubuntu
assert_call nimf "-e IM_FRAMEWORK=nimf"
assert_call nimf "-e INPUT_LANGUAGE=ja"
assert_call nimf "-t nimf-latency-ubuntu:test"
assert_no_call nimf "Dockerfile.frameworks"
check 0 profile ubuntu PROFILE=1
assert_call profile "-t nimf-latency-ubuntu:profile"
assert_call profile "--build-arg NIMF_BUILD_PROFILING=1"
assert_no_call profile "Dockerfile.frameworks"
check 0 ibus ubuntu IM_FRAMEWORK=ibus
assert_call ibus "-e IM_FRAMEWORK=ibus"
assert_call ibus "-f tests/latency/Dockerfile.frameworks.ubuntu"
assert_call ibus "--build-arg BASE_IMAGE=nimf-latency-ubuntu:test"
assert_call ibus "-t nimf-latency-ubuntu:frameworks-ibus"
check 0 arch-fcitx5 arch IM_FRAMEWORK=fcitx5 INPUT_LANGUAGE=ko
assert_call arch-fcitx5 "-f tests/latency/Dockerfile.frameworks.arch"
assert_call arch-fcitx5 "--build-arg INPUT_LANGUAGE=ko"
assert_call arch-fcitx5 "-t nimf-latency-arch:frameworks-fcitx5"
assert_call arch-fcitx5 "-e IM_FRAMEWORK=fcitx5"
assert_call arch-fcitx5 "-e INPUT_LANGUAGE=ko"
check 0 upstream ubuntu IM_FRAMEWORK=upstream INPUT_LANGUAGE=ko
assert_call upstream "-e IM_FRAMEWORK=upstream"
assert_call upstream "-e INPUT_LANGUAGE=ko"
assert_call upstream "-f tests/latency/Dockerfile.upstream"
assert_call upstream "--build-arg BASE_IMAGE=nimf-latency-ubuntu:test"
assert_call upstream "--build-arg UPSTREAM_REVISION=b4a141aec1c833541f163da8b1beb9ab1e0c0615"
assert_call upstream "-t nimf-latency-ubuntu:upstream"
assert_no_call upstream "Dockerfile.frameworks"
check 2 upstream-ja ubuntu IM_FRAMEWORK=upstream INPUT_LANGUAGE=ja
assert_no_docker upstream-ja
assert_stderr upstream-ja "no Mozc engine"
check 2 bad-framework ubuntu IM_FRAMEWORK=m17n
assert_no_docker bad-framework
check 2 bad-language ubuntu INPUT_LANGUAGE=en
assert_no_docker bad-language
check 2 profile-ibus ubuntu PROFILE=1 IM_FRAMEWORK=ibus
assert_no_docker profile-ibus
assert_stderr profile-ibus "Profiling supports nimf only"
check 2 profile-fcitx5 arch PROFILE=1 IM_FRAMEWORK=fcitx5
assert_no_docker profile-fcitx5
check 0 cpuset ubuntu CPUSET=0-3
assert_call cpuset "--cpuset-cpus 0-3"
check 2 bad-cpuset ubuntu CPUSET=0-3,all
assert_no_docker bad-cpuset
assert_stderr bad-cpuset "CPUSET must list CPU numbers"
assert_no_call nimf "--cpuset-cpus"
actual=0
env PROFILE=1 IM_FRAMEWORK=ibus bash tests/latency/session.sh \
    > "$output/session.out" 2> "$output/session.err" || actual=$?
if [ "$actual" != 2 ]; then
    echo "FAIL: session guard expected exit 2, got $actual" >&2
    status=1
fi
assert_stderr session "Profiling supports nimf only"
actual=0
env IM_FRAMEWORK=upstream INPUT_LANGUAGE=ja bash tests/latency/session.sh \
    > "$output/session-upstream.out" 2> "$output/session-upstream.err" || actual=$?
if [ "$actual" != 2 ]; then
    echo "FAIL: session upstream guard expected exit 2, got $actual" >&2
    status=1
fi
assert_stderr session-upstream "no Mozc engine"
if [ "$status" = 0 ]; then
    echo "PASS: latency runner contract checks"
fi
exit "$status"
