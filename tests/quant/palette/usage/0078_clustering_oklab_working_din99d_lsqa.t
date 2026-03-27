#!/bin/sh
# Verify MS-SSIM for clustering oklab working din99d colorspace selection.
#
# Flow summary:
# - Convert the input image with -X oklab -W din99d.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.975 via lsqa.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.975

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-oklab-work-din99d.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -t rgb -X oklab -W din99d -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel clustering oklab working din99d conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "clustering oklab working din99d lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "clustering oklab working din99d lsqa failed"

exit 0
