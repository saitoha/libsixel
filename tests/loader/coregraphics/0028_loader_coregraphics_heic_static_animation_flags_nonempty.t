#!/bin/sh
# TAP test: static HEIC decode remains non-empty under animation CLI flags.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -L coregraphics! -u -g \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heic_static_with_anim_flags.six" || {
    echo "not ok" 1 "coregraphics HEIC static decode with animation flags failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/coregraphics_heic_static_with_anim_flags.six" || {
    echo "not ok" 1 "coregraphics HEIC static decode with animation flags is empty"
    exit 0
}

echo "ok" 1 "coregraphics HEIC static decode with animation flags is non-empty"
exit 0
