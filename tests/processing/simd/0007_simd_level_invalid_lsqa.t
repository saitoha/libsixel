#!/bin/sh
# Validate SIXEL_SIMD_LEVEL=invalid path with scale+colorspace work and LSQA.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test -x "${IMG2SIXEL_PATH}" || skip_all "img2sixel is disabled in this build"
test -x "${LSQA_PATH}" || skip_all "lsqa is disabled in this build"
echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64_jpg_resize_63x.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_63_invalid.six"
lsqa_run_status=0
lsqa_err=""

run_img2sixel --env SIXEL_SIMD_LEVEL=definitely_invalid -Wo -w63 -o "${output_sixel}" "${input_image}" || {
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
