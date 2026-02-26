#!/bin/sh
# TAP test: coregraphics loader keeps MS-SSIM baseline for RLE RGB TGA input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

lsqa_floor=0.98
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type10-rgb.tga"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/coregraphics_tga_rle_rgb.six"

run_img2sixel -L coregraphics! "${image_path}" >"${output_sixel}" || {
    fail 1 "coregraphics failed to decode RLE RGB TGA input"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "coregraphics keeps MS-SSIM baseline for RLE RGB TGA input"
exit 0
