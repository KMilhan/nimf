#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
output=$(mktemp -d "$PWD/build/profile-check-XXXXXX")
read -r -a flags <<< "$(pkg-config --cflags --libs glib-2.0)"
read -r -a extra <<< "${PROFILE_TEST_CFLAGS:-}"
for mode in enabled disabled; do
    options=()
    if [[ $mode == enabled ]]; then options=(-DNIMF_ENABLE_PROFILING=1); fi
    "${CC:-cc}" -g -O2 -Wall -Wextra -Werror "${extra[@]}" "${options[@]}" -Ilibnimf \
        tests/profile/fixture.c libnimf/nimf-profile.c -o "$output/$mode" "${flags[@]}"
done
PROFILE_TEST_BIN="$output" PYTHONDONTWRITEBYTECODE=1 python3 tests/profile/test_profile.py
echo "Fixture binaries: $output"
