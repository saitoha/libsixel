#!/bin/sh
# Verify parse error when baseline value has trailing garbage characters.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_trailing_garbage.err"

set +e
run_lsqa -b "MS-SSIM:1.0x" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    fail 1 "baseline trailing garbage was not rejected as expected"
    exit 0
}

grep -F "Unexpected characters in value" "${err_file}" >/dev/null || {
    fail 1 "baseline trailing garbage was not rejected as expected"
    exit 0
}

pass 1 "baseline trailing garbage was rejected"

exit 0
