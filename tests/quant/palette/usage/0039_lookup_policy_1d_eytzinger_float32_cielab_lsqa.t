#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the CIELAB colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.979487
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.97}


input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-cielab.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=eytzinger --working-colorspace=cielab -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "float32 Eytzinger CIELAB colorspace conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "float32 Eytzinger CIELAB colorspace lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "float32 Eytzinger CIELAB colorspace lsqa failed"

exit 0
