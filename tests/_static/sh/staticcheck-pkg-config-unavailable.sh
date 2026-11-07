#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Both consumers must skip configure's unavailable-tool sentinel before use.
set -eux
echo '1..1'
set -v
# shellcheck disable=SC2016
awk '
function valid(tap, consumer, guard, command) {
    guard = index(consumer, "if not pkgconfig or pkgconfig == \047false\047:\n        return \047pkg-config is unavailable\047")
    command = index(consumer, "pkgconfig_command = [pkgconfig]")
    return guard && command > guard &&
        index(tap, "test \"${SIXEL_TEST_PKG_CONFIG}\" != false || {\n    echo \"1..0 # SKIP pkg-config is unavailable\"\n    exit 0\n}")
}
FILENAME == ARGV[1] { tap = tap $0 "\n" }
FILENAME == ARGV[2] { consumer = consumer $0 "\n" }
END {
    missing = consumer
    sub(/ or pkgconfig == \047false\047/, "", missing)
    inverted = tap
    sub(/!= false/, "= false", inverted)
    exit !valid(tap, consumer) || valid(tap, missing) ||
        valid(inverted, consumer)
}
' "$1/tests/build/pkgconfig/0001_libsixel_pc_resolves.t" \
    "$1/tests/build/install/0001_pkgconfig_headers.py" || {
    echo 'not ok 1 - unavailable pkg-config is skipped before invocation'
    exit 0
}
echo 'ok 1 - unavailable pkg-config is skipped before invocation'
