#!/bin/sh
# Expect noisy JPEG headers to be rejected or graded as low quality.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_image="${TOP_SRCDIR}/tests/data/corrupted/metadata_noise.jpg"

lsqa_msg=$(
    run_lsqa -m MS-SSIM -b "MS-SSIM:0.5" "${input_image}" "${input_image}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status-0}" -ne 0 || {
    echo "not ok" 1 - "metadata noise unexpectedly accepted, ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "metadata noise rejected or scored low"
exit 0
