#!/bin/sh
# Verify parse error when -b is specified more than once.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_duplicate_baseline.err"

set +e
run_lsqa -b "MS-SSIM:0.0" -b "MS-SSIM:0.0" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "duplicate -b option was not rejected as expected"
    exit 0
}

grep "baseline already specified" "${err_file}" >/dev/null || {
    echo "not ok" 1 - "duplicate -b option was not rejected as expected"
    exit 0
}

echo "ok" 1 - "duplicate -b option was rejected"

exit 0
