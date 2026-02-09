#!/bin/sh
# TAP test confirming sixel2png accepts an unambiguous dequantize prefix.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png -dk_ <"${images_dir}/map8.six" \
        >"${ARTIFACT_LOCAL_DIR}/dequantize-short.png" \
       2>"${ARTIFACT_LOCAL_DIR}/err.txt" || {
    fail 1 "unique dequantize prefix rejected"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/dequantize-short.png" || {
    fail 1 "unexpected diagnostics for -dk_"
    exit 0
}

pass 1 "accepts unique dequantize prefix"
exit 0
