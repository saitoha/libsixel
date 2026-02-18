#!/bin/sh
# Run lsqa checks for float32 FHEDT in the DIN99d colorspace.
# The lsqa helper can read SIXEL directly, so compare with SIXEL output.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.97
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/fhedt-float32-din99d.six"

run_img2sixel --lookup-policy=fhedt --precision=float32 \
    --working-colorspace=din99d -o "${output_sixel}" "${input_image}" || {
    fail 1 "float32 FHEDT din99d colorspace conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "float32 FHEDT din99d colorspace lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "float32 FHEDT din99d colorspace lsqa failed"

exit 0
