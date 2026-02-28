#!/bin/sh
# Verify parse error when baseline value is empty.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_empty_value.err"

set +e
run_lsqa -b "MS-SSIM:" "${image_ref}" >/dev/null 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 "empty baseline value was not rejected as expected"
    exit 0
}

grep -q "Baseline value is empty" "${err_file}" || {
    echo "not ok" 1 "empty baseline value was not rejected as expected"
    exit 0
}

echo "ok" 1 "empty baseline value was rejected"

exit 0
