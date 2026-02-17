#!/bin/sh
# Validate SIXEL_SIMD_LEVEL=invalid path with scale+colorspace work and LSQA.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test -x "${IMG2SIXEL_PATH}" || skip_all "img2sixel is disabled in this build"
test -x "${LSQA_PATH}" || skip_all "lsqa is disabled in this build"
magick_cmd=magick
command -v "${magick_cmd}" >/dev/null 2>&1 || magick_cmd=convert
command -v "${magick_cmd}" >/dev/null 2>&1 || skip_all "ImageMagick unavailable"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
reference_image="${ARTIFACT_LOCAL_DIR}/snake_63_ref_invalid.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_63_invalid.six"
lsqa_run_status=0
lsqa_err=""

"${magick_cmd}" "${input_image}" -resize 63x "${reference_image}" || {
    fail 1 "reference resize failed"
    exit 0
}
run_img2sixel --env SIXEL_SIMD_LEVEL=definitely_invalid -Wo -w63     -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel failed"
    exit 0
}

lsqa_err=$(run_lsqa -b "MS-SSIM:0.99" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    pass 1 "SIXEL_SIMD_LEVEL=invalid reached MS-SSIM 0.99"
    exit 0
}

lsqa_run_status=0
lsqa_err=$(run_lsqa -b "MS-SSIM:0.98" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    pass 1 "SIXEL_SIMD_LEVEL=invalid reached MS-SSIM 0.98"
    exit 0
}

fail 1 "SIXEL_SIMD_LEVEL=invalid fell below MS-SSIM 0.98: ${lsqa_err}"
exit 0
