#!/bin/sh
# TAP test: builtin GIF start frame accepts negative indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/builtin_gif_start_default_neg.six" || {
    echo "not ok" 1 - "baseline builtin GIF decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=-1" \
    -L builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/builtin_gif_start_negative.six" || {
    echo "not ok" 1 - "builtin GIF decode with negative start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/builtin_gif_start_default_neg.six" \
    "${ARTIFACT_LOCAL_DIR}/builtin_gif_start_negative.six" && {
    echo "not ok" 1 - "negative start frame did not change builtin GIF output"
    exit 0
}

echo "ok" 1 - "builtin GIF negative start frame is applied"
exit 0
