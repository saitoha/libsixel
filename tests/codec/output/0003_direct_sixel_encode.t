#!/bin/sh
# Encode existing Sixel data directly.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-direct.sixel"

run_img2sixel -e "${snake_six}" >"${target_sixel}" || {
    echo "not ok" 1 "direct Sixel encode failed"
    exit 0
}

echo "ok" 1 "direct Sixel encode emits data"
exit 0
