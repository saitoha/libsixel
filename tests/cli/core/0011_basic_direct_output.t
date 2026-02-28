#!/bin/sh
# TAP test producing direct RGBA output with sixel2png.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

direct_png="${ARTIFACT_LOCAL_DIR}/output.png"
run_sixel2png -D <"${TOP_SRCDIR}/images/map8.six" >"${direct_png}" || {
    fail 1 "direct RGBA conversion failed"
    exit 0
}

pass 1 "produces direct RGBA output"
exit 0
