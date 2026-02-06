#!/bin/sh
# Expect truncated PNG input to be rejected or graded as low quality.

set -eu

. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

printf '1..1\n'
set -v

input_image="${top_srcdir}/tests/data/corrupted/truncated.png"

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:0.5" "${input_image}" "${input_image}"
) || lsqa_run_status=$?

test -z "${lsqa_run_status-}" || {
    pass 1 "truncated input rejected or scored low"
    exit 0
}

fail 1 "truncated input unexpectedly accepted"
exit 0
