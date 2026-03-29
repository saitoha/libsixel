#!/bin/sh
# Run all loader/libwebp TAP tests with a stable local environment.

set -eu

src_root=${1:-$(cd "$(dirname "$0")/../../.." && pwd)}
build_root=${2:-$src_root}

test -n "${SIXEL_TEST_PHP:-}" || {
    command -v php >/dev/null 2>&1 &&
        SIXEL_TEST_PHP=$(command -v php) &&
        export SIXEL_TEST_PHP
}

set -- "$src_root"/tests/loader/libwebp/*.t
test -f "$1" || {
    printf '%s\n' 'no loader/libwebp TAP tests were found' >&2
    exit 1
}

webp_tests=$(printf '%s\n' "$@" | LC_ALL=C sort | tr '\n' ' ')

exec make -C "$build_root/tests" check TESTS="$webp_tests"
