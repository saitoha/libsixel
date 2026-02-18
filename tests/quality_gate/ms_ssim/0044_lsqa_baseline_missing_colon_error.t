#!/bin/sh
# Verify parse error for -b values without METRIC:VALUE format.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_missing_colon.err"

set +e
run_lsqa -b "MS-SSIM" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    fail 1 "baseline without colon was not rejected as expected"
    exit 0
}

grep "Expected METRIC:VALUE format" "${err_file}" >/dev/null || {
    fail 1 "baseline without colon was not rejected as expected"
    exit 0
}

pass 1 "baseline without colon was rejected"

exit 0
