#!/bin/bash
set -euo pipefail
# shellcheck source=tests/memory/flags.sh
source /src/tests/memory/flags.sh
cp /src/memory-analysis.log /results/analysis.log
cp /src/libnimf/test-memory.log /results/core-unit.log
if [[ $NIMF_CHECK_MODE == analyze ]]; then
    echo 'PASS static analysis and unit tests'
    exit
fi
export ASAN_OPTIONS="$ASAN_OPTIONS:log_path=/results/asan"
export NO_AT_BRIDGE=1
status=0
bash /src/tests/mozc/session.sh || status=1
profile=$(mktemp -d /tmp/nimf-memory-XXXXXX)
mkdir -m 700 "$profile/config" "$profile/runtime"
xvfb-run -a -s '-screen 0 1280x800x24 -nolisten tcp' \
    env XDG_CONFIG_HOME="$profile/config" XDG_RUNTIME_DIR="$profile/runtime" \
    dbus-run-session -- bash /src/tests/memory/lifecycle.sh || status=1
if grep -Erq 'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:' /results; then
    echo 'FAIL: sanitizer diagnostics' >&2
    status=1
fi
exit "$status"
