#!/bin/sh
# TAP test verifying png:- writes PNG data to stdout.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

png_stdout="${ARTIFACT_LOCAL_DIR}/png-stdout.png"

run_sixel2png -o "png:-" <"${TOP_SRCDIR}/images/map8.six" >"${png_stdout}" || {
    fail 1 "png:- command failed"
    exit 0
}

test -s "${png_stdout}" || {
    fail 1 "png:- produced empty output"
    exit 0
}

pass 1 "png:- writes to stdout"
exit 0
