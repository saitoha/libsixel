#!/bin/sh
# TAP test: libpng should decode 16-bit sRGB PNG (without iCCP/gAMA/cHRM)
# and keep visual parity against the 64x64 PNM reference.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/libpng_expected/0141_libpng_rgb16_srgb_only_64x64_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_64_rgb16_srgb_only.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.99" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "libpng rgb16 sRGB-only fixture matches 64x64 reference"
exit 0
