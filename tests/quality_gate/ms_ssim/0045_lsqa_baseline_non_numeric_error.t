#!/bin/sh
# Verify parse error for non-numeric -b baseline values.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_non_numeric.err"

run_lsqa -b "MS-SSIM:not_a_number" "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "Baseline value is not a number" "${err_file}" >/dev/null; then
    pass 1 "non-numeric baseline was rejected"
else
    fail 1 "non-numeric baseline was not rejected as expected"
fi

exit 0
