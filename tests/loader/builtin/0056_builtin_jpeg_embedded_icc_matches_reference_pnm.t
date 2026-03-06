#!/bin/sh
# Verify builtin JPEG ICC conversion matches a fixed no-ICC PNM reference.

set -eux

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
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

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0002_snake_64_embedded_esrgb_converted_srgb_noicc.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake-64-builtin-jpeg-icc.sixel"

run_img2sixel -Lbuiltin! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin decode failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${reference_ppm}" "${output_sixel}" >/dev/null || {
    echo "not ok" 1 - "builtin output mismatched converted reference"
    exit 0
}

echo "ok" 1 - "builtin JPEG ICC conversion matches fixed PNM reference"
exit 0
