#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the CIELAB colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.979487
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.97}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-cielab.six"

run_img2sixel --lookup-policy=eytzinger --working-colorspace=cielab -o "${output_sixel}" "${input_image}" || {
    fail 1 "float32 Eytzinger CIELAB colorspace conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "float32 Eytzinger CIELAB colorspace lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "float32 Eytzinger CIELAB colorspace lsqa failed"

exit 0
