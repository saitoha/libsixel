#!/bin/sh
# Verify linear background interpretation changes libwebp alpha composition.

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
output_gamma="${ARTIFACT_LOCAL_DIR}/webp-bgcs-alpha-gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/webp-bgcs-alpha-linear.six"

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibwebp:cms=0! \
              -S \
              -B#808080 "${input_webp}" >"${output_gamma}" || {
    echo "not ok" 1 - "libwebp gamma background composition failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Llibwebp:cms=0! \
              -S \
              -B#808080 "${input_webp}" >"${output_linear}" || {
    echo "not ok" 1 - "libwebp linear background composition failed"
    exit 0
}

if cmp -s "${output_gamma}" "${output_linear}"; then
    echo "not ok" 1 - "gamma and linear composition produced identical output"
    exit 0
fi

echo "ok" 1 - "linear background interpretation changes libwebp composition"
exit 0
