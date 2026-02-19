#!/bin/sh
# TAP test converting map64.six using explicit stdin/stdout arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_sixel2png - - <"${TOP_SRCDIR}/images/map64.six" \
        >"${ARTIFACT_LOCAL_DIR}/map64-stdin-stdout.png" || {
    fail 1 "map64 stdin/stdout conversion failed"
    exit 0
}

pass 1 "converts map64 with explicit stdin/stdout"
exit 0
