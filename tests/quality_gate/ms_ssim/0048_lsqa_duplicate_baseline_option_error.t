#!/bin/sh
# Verify parse error when -b is specified more than once.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_duplicate_baseline.err"

run_lsqa -b "MS-SSIM:0.0" -b "MS-SSIM:0.0" "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "baseline already specified" "${err_file}" >/dev/null; then
    pass 1 "duplicate -b option was rejected"
else
    fail 1 "duplicate -b option was not rejected as expected"
fi

exit 0
