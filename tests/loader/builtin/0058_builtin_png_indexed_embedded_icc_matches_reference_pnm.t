#!/bin/sh
# Verify builtin indexed PNG ICC conversion matches a fixed no-ICC PNM reference.

set -eux

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-indexed-embedded-esrgb.png"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0003_snake_64_indexed_embedded_esrgb_converted_srgb_noicc.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake-64-builtin-indexed-png-icc.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${reference_ppm}" "${output_sixel}" >/dev/null || {
    echo "not ok" 1 - "builtin output mismatched converted reference"
    exit 0
}

echo "ok" 1 - "builtin indexed PNG ICC conversion matches fixed PNM reference"
exit 0
