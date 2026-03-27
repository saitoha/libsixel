#!/bin/sh
# TAP test: libwebp animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_default.six" || {
    echo "not ok" 1 - "baseline libwebp animation decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_start_positive.six" || {
    echo "not ok" 1 - "libwebp decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/webp_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/webp_start_positive.six" && {
    echo "not ok" 1 - "positive start frame did not change libwebp output"
    exit 0
}

echo "ok" 1 - "libwebp positive start frame is applied"
exit 0
