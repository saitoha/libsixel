#!/bin/sh
# TAP test: builtin GIF start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

run_img2sixel -L builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/builtin_gif_start_default.six" || {
    echo "not ok" 1 - "baseline builtin GIF decode failed"
    exit 0
}

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -L builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/builtin_gif_start_positive.six" || {
    echo "not ok" 1 - "builtin GIF decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/builtin_gif_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/builtin_gif_start_positive.six" && {
    echo "not ok" 1 - "positive start frame did not change builtin GIF output"
    exit 0
}

echo "ok" 1 - "builtin GIF positive start frame is applied"
exit 0
