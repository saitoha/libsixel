#!/bin/sh
# Verify builtin PNG ICC conversion matches a fixed no-ICC PNM reference.

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

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0001_map8_embedded_icc_converted_srgb_noicc.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/map8-builtin-icc.sixel"

run_img2sixel -Lbuiltin:cms_engine=auto! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin decode failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${reference_ppm}" "${output_sixel}" >/dev/null || {
    echo "not ok" 1 - "builtin output mismatched converted reference"
    exit 0
}

echo "ok" 1 - "builtin ICC conversion matches fixed PNM reference"
exit 0
