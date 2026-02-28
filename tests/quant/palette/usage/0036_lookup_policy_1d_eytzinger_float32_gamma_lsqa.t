#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the gamma colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.976111
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-gamma.six"

run_img2sixel --lookup-policy=eytzinger --precision=float32 --working-colorspace=gamma -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 "float32 Eytzinger gamma colorspace conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 "float32 Eytzinger gamma colorspace lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 "${lsqa_err}"
    exit 0
}

echo "not ok" 1 "float32 Eytzinger gamma colorspace lsqa failed"
exit 0
