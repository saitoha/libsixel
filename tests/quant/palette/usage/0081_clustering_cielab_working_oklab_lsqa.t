#!/bin/sh
# Verify MS-SSIM for clustering cielab working oklab colorspace selection.
#
# Flow summary:
# - Convert the input image with -X cielab -W oklab.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.98


input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-cielab-work-oklab.six"

run_img2sixel -t rgb -X cielab -W oklab -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel clustering cielab working oklab conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "clustering cielab working oklab lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "clustering cielab working oklab lsqa failed"

exit 0
