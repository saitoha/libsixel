#!/bin/sh
# Expect truncated PNG input to be rejected or graded as low quality.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_image="${top_srcdir}/tests/data/corrupted/truncated.png"
lsqa_run_status=0

run_quiet run_lsqa -b "MS-SSIM:0.5" "${input_image}" "${input_image}" || lsqa_run_status=$?

test "${lsqa_run_status}" -ne 0 || {
    fail 1 "truncated input unexpectedly accepted"
    exit 0
}

pass 1 "truncated input rejected or scored low"
exit 0
