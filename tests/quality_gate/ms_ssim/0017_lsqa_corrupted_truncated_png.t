#!/bin/sh
# Expect truncated PNG input to be rejected or graded as low quality.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_image="${TOP_SRCDIR}/tests/data/corrupted/truncated.png"
lsqa_run_status=0

lsqa_msg=$(
    run_lsqa -m MS-SSIM -b "MS-SSIM:0.5" "${input_image}" "${input_image}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status-0}" -ne 0 || {
    fail 1 "truncated input unexpectedly accepted, ${lsqa_msg}"
    exit 0
}

pass 1 "truncated input rejected or scored low"
exit 0
