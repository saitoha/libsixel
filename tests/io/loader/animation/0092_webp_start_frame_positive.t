#!/bin/sh
# TAP test: libwebp animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_default.six" || {
    fail 1 "baseline libwebp animation decode failed"
    exit 0
}

run_img2sixel --start-frame=1 -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_positive.six" || {
    fail 1 "libwebp decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/webp_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/webp_start_positive.six" && {
    fail 1 "positive start frame did not change libwebp output"
    exit 0
}

pass 1 "libwebp positive start frame is applied"
exit 0
