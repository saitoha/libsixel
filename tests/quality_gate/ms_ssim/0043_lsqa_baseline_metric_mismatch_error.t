#!/bin/sh
# Verify parse error when baseline metric does not match -m metric.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_mismatch.err"

set +e
run_lsqa -m GMSD -b "MS-SSIM:0.0" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "mismatched baseline metric was not rejected as expected"
    exit 0
}

grep "baseline metric must match -m" "${err_file}" >/dev/null || {
    echo "not ok" 1 - "mismatched baseline metric was not rejected as expected"
    exit 0
}

echo "ok" 1 - "mismatched baseline metric was rejected"

exit 0
