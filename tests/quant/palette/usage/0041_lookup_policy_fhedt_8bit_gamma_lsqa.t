#!/bin/sh
# Run lsqa checks for 8-bit FHEDT in the gamma colorspace.
# The lsqa helper can read SIXEL directly, so compare with SIXEL output.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.97
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.97}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/fhedt-8bit-gamma.six"

run_img2sixel --lookup-policy=fhedt -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "8-bit FHEDT gamma colorspace conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "8-bit FHEDT gamma colorspace lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "8-bit FHEDT gamma colorspace lsqa failed"

exit 0
