#!/bin/sh
# Validate SIXEL_SIMD_LEVEL=auto path with scale+colorspace work and LSQA.

# The independently resized reference remains >=176x176.
# Calculate only the metric asserted by this quality gate.
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

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_192.jpg"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_192_jpg_resize_191x.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_191_auto.six"
lsqa_run_status=0
lsqa_err=""

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_SIMD_LEVEL=auto -Wo -w191 -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_err=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.99" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=auto reached MS-SSIM 0.99"
    exit 0
}

lsqa_run_status=0
lsqa_err=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${reference_image}" "${output_sixel}" 2>&1) || lsqa_run_status=$?
test "${lsqa_run_status}" -eq 0 && {
    echo "ok" 1 - "SIXEL_SIMD_LEVEL=auto reached MS-SSIM 0.98"
    exit 0
}

echo "not ok" 1 - "SIXEL_SIMD_LEVEL=auto fell below MS-SSIM 0.98: ${lsqa_err}"
exit 0
