#!/bin/sh
# Inspect Sixel with X ordered dither configuration.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection-x-dither.txt"

run_img2sixel -I -dx_dither -h100 "${snake_six}" >"${target_txt}" || {
    echo "not ok" 1 "X ordered dither inspection fails"
    exit "${status}"
}

echo "ok" 1 "X ordered dither inspection works"
exit "${status}"
