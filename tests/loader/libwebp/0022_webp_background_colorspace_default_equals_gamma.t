#!/bin/sh
# Verify SIXEL_LOADER_BACKGROUND_COLORSPACE defaults to gamma for libwebp.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-min.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-bgcs-default.six"
output_gamma="${ARTIFACT_LOCAL_DIR}/webp-bgcs-gamma.six"

run_img2sixel -Llibwebp:cms=0! -S -B#808080 "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp default background colorspace decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibwebp:cms=0! \
              -S \
              -B#808080 "${input_webp}" >"${output_gamma}" || {
    echo "not ok" 1 - "libwebp gamma background colorspace decode failed"
    exit 0
}

cmp -s "${output_default}" "${output_gamma}" || {
    echo "not ok" 1 - "default background colorspace does not match gamma"
    exit 0
}

echo "ok" 1 - "default background colorspace matches gamma for libwebp"
exit 0
