#!/bin/sh
# Validate SIXEL_SIMD_LEVEL=avx path with scale+colorspace work and LSQA.

set -eux


test -x "${IMG2SIXEL_PATH}" || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test -x "${LSQA_PATH}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n";
    exit 0
}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64_jpg_resize_63x.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_63_avx.six"
lsqa_run_status=0
lsqa_err=""

run_img2sixel --env SIXEL_SIMD_LEVEL=avx -Wo -w63 -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_err=$(run_lsqa -b "MS-SSIM:0.99" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=avx reached MS-SSIM 0.99"
    exit 0
}

lsqa_run_status=0
lsqa_err=$(run_lsqa -b "MS-SSIM:0.98" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=avx reached MS-SSIM 0.98"
    exit 0
}

echo "not ok" 1 - "SIXEL_SIMD_LEVEL=avx fell below MS-SSIM 0.98: ${lsqa_err}"
exit 0
