#!/bin/bash
# Sourced by the isolated build and runtime entry points.
case ${NIMF_CHECK_MODE:-sanitize} in
    sanitize)
        export CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
        export CXXFLAGS="$CFLAGS"
        export LDFLAGS='-fsanitize=address,undefined'
        export ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'
        export UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'
        export LSAN_OPTIONS='suppressions=/src/tests/memory/lsan.supp:print_suppressions=1'
        ;;
    analyze)
        export CFLAGS='-O1 -g -fanalyzer'
        export CXXFLAGS='-O1 -g'
        export LDFLAGS=''
        ;;
    *) echo 'Expected sanitize or analyze' >&2; exit 2 ;;
esac
export CFLAGS="$CFLAGS -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=return-type"
export CXXFLAGS="$CXXFLAGS -Werror=return-type"
