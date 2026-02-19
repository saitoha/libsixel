#!/bin/sh
# TAP test converting snake.six from stdin with sixel2png.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_sixel2png - <"${TOP_SRCDIR}/images/map8.six" >"${ARTIFACT_LOCAL_DIR}/snake-stdin.png" || {
    fail 1 "snake stdin conversion failed"
    exit 0
}

pass 1 "converts snake from stdin"
exit 0
