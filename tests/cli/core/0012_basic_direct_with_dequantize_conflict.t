#!/bin/sh
# TAP test ensuring sixel2png rejects mixing direct output with dequantize flags.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

direct_err="${ARTIFACT_LOCAL_DIR}/err.txt"

run_sixel2png -D -dk_undither <"${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
        >"${ARTIFACT_LOCAL_DIR}/output.txt" 2>"${direct_err}" && {
    fail 1 "accepts conflicting direct/dequantize flags"
    exit 0
}

grep -F "cannot be combined" "${direct_err}" >/dev/null || {
    fail 1 "missing direct/dequantize diagnostic"
    exit 0
}

pass 1 "rejects direct/dequantize mix"
exit 0
