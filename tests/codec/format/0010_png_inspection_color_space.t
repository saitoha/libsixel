#!/bin/sh
# Verify PNG inspection sets colour space in the report.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/png-inspection.txt"

run_img2sixel -I -C10 -djajuni "${snake_png}" >"${target_txt}" || {
    echo "not ok" 1 - "PNG inspection colour space failed"
    exit 0
}

echo "ok" 1 - "PNG inspection sets colour space"

exit 0
