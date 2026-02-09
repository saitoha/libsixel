#!/bin/sh
# Verify parse error when baseline metric does not match -m metric.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_mismatch.err"

run_lsqa -m GMSD -b "MS-SSIM:0.0" "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "baseline metric must match -m" "${err_file}" >/dev/null; then
    pass 1 "mismatched baseline metric was rejected"
else
    fail 1 "mismatched baseline metric was not rejected as expected"
fi

exit 0
