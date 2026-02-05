#!/bin/sh
# TAP test covering the -R/--gri-limit option with deterministic encoding.
#
# Flow:
# - Encode a reference input with a single encoder thread (-=1).
# - Encode the same input with -R/--gri-limit enabled and -=1.
# - Compare outputs with lsqa MS-SSIM and require a perfect match.

set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${conversion_common_path}"
. "${lsqa_common_path}"

status=0

lsqa_floor=1.0

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
case_id=${test_name%.t}
output_plain="${ARTIFACT_LOCAL_DIR}/${case_id}-plain.six"
output_limited="${ARTIFACT_LOCAL_DIR}/${case_id}-limited.six"



run_img2sixel -=1 -o "${output_plain}" "${input_image}" || {
    fail 1 "img2sixel failed"
}
run_img2sixel -=1 --gri-limit -o "${output_limited}" "${input_image}" || {
    fail 1 "img2sixel failed"
}
lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${output_plain}" "${output_limited}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "gri-limit deterministic output matches"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "gri-limit deterministic output mismatch"
fi

exit "${status}"
