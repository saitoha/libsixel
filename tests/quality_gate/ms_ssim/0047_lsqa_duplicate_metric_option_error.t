#!/bin/sh
# Verify parse error when -m is specified more than once.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_duplicate_metric.err"

set +e
run_lsqa -m MS-SSIM -m GMSD "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "duplicate -m option was not rejected as expected"
    exit 0
}

grep "metric already specified" "${err_file}" >/dev/null || {
    echo "not ok" 1 - "duplicate -m option was not rejected as expected"
    exit 0
}

echo "ok" 1 - "duplicate -m option was rejected"

exit 0
