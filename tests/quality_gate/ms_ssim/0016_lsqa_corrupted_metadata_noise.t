#!/bin/sh
# Expect noisy JPEG headers to be rejected or graded as low quality.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

printf '1..1\n'
set -v

input_image="${top_srcdir}/tests/data/corrupted/metadata_noise.jpg"

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:0.5" "${input_image}" "${input_image}"
) || lsqa_run_status=$?

test -z "${lsqa_run_status-}" || {
    pass 1 "metadata noise rejected or scored low"
    exit 0
}

fail 1 "metadata noise unexpectedly accepted"
exit 0
