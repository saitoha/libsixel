#!/bin/sh
# Ensure indexed PNG scales to a larger width.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_palette_png="${TOP_SRCDIR}/images/snake-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/indexed-scale.sixel"

run_img2sixel -7 -w300 "${snake_palette_png}" \
        >"${target_sixel}" || {
    echo "not ok" 1 "indexed PNG scaling fails"
    exit 0
}

echo "ok" 1 "indexed PNG scales to large width"

exit 0
