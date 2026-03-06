#!/bin/sh
# TAP test converting map8.six from stdin with sixel2png.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_sixel2png - <"${TOP_SRCDIR}/images/map8.six" >/dev/null || {
    echo "not ok" 1 - "map8 stdin conversion failed"
    exit 0
}

echo "ok" 1 - "converts map8 from stdin"
exit 0
