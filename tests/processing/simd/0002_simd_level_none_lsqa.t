#!/bin/sh
# Validate SIXEL_SIMD_LEVEL=none path with scale+colorspace work and LSQA.

set -eux


test -x "${IMG2SIXEL_PATH}" || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test -x "${LSQA_PATH}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n";
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64_jpg_resize_63x.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_63_none.six"
lsqa_run_status=0
lsqa_err=""

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_SIMD_LEVEL=none -Wo -w63 -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_err=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:0.99" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=none reached MS-SSIM 0.99"
    exit 0
}

lsqa_run_status=0
lsqa_err=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:0.98" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=none reached MS-SSIM 0.98"
    exit 0
}

echo "not ok" 1 - "SIXEL_SIMD_LEVEL=none fell below MS-SSIM 0.98: ${lsqa_err}"
exit 0
