#!/bin/sh
# TAP test ensuring sixel2png rejects mixing direct output with dequantize flags.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

direct_err="${ARTIFACT_LOCAL_DIR}/err.txt"

run_sixel2png -D -dk_undither <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
        >"${ARTIFACT_LOCAL_DIR}/output.txt" 2>"${direct_err}" && {
    echo "not ok" 1 "accepts conflicting direct/dequantize flags"
    exit 0
}

grep "cannot be combined" "${direct_err}" >/dev/null || {
    echo "not ok" 1 "missing direct/dequantize diagnostic"
    exit 0
}

echo "ok" 1 "rejects direct/dequantize mix"
exit 0
