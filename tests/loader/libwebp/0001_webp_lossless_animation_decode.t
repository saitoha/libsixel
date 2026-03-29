#!/bin/sh
# TAP test: libwebp loop-disable animation output matches static reference stream.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! -=1 -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp" \
    >"${ARTIFACT_LOCAL_DIR}/webp_loop_disable.six" || {
    echo "not ok" 1 - "libwebp lossless animation decode failed"
    exit 0
}

cmp -s \
    "${TOP_SRCDIR}/tests/data/inputs/formats/animated_lossless_8x8_2frame_min_loop_disable_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/webp_loop_disable.six" || {
    echo "not ok" 1 - "libwebp loop-disable animation output differed from reference"
    exit 0
}

echo "ok" 1 - "libwebp loop-disable animation output matched static reference"
exit 0
