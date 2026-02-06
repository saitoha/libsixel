#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the DIN99d colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.970762
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-din99d.six"
output_png="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-din99d.png"

if run_img2sixel --lookup-policy=eytzinger --precision=float32 \
        --working-colorspace=din99d \
    -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "float32 Eytzinger DIN99d colorspace conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "float32 Eytzinger DIN99d colorspace lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "float32 Eytzinger DIN99d colorspace lsqa failed"
fi

exit "${status}"
