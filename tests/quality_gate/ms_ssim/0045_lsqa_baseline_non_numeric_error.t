#!/bin/sh
# Verify parse error for non-numeric -b baseline values.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_non_numeric.err"

set +e
run_lsqa -b "MS-SSIM:not_a_number" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "non-numeric baseline was not rejected as expected"
    exit 0
}

grep "Baseline value is not a number" "${err_file}" >/dev/null || {
    echo "not ok" 1 - "non-numeric baseline was not rejected as expected"
    exit 0
}

echo "ok" 1 - "non-numeric baseline was rejected"

exit 0
