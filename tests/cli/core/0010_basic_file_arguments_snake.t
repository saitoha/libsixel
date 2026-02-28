#!/bin/sh
# TAP test converting snake.six with explicit file arguments.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_sixel2png -i "${TOP_SRCDIR}/images/map8.six" -o "${ARTIFACT_LOCAL_DIR}/output.png" || {
    fail 1 "snake file conversion failed"
    exit 0
}

pass 1 "converts snake with file arguments"
exit 0
