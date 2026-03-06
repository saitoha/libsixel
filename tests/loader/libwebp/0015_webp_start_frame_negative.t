#!/bin/sh
# TAP test: libwebp animation start frame accepts negative indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

run_img2sixel -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_default_neg.six" || {
    echo "not ok" 1 - "baseline libwebp animation decode failed"
    exit 0
}

run_img2sixel --start-frame=-1 -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_negative.six" || {
    echo "not ok" 1 - "libwebp decode with negative start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/webp_start_default_neg.six" \
    "${ARTIFACT_LOCAL_DIR}/webp_start_negative.six" && {
    echo "not ok" 1 - "negative start frame did not change libwebp output"
    exit 0
}

echo "ok" 1 - "libwebp negative start frame is applied"
exit 0
