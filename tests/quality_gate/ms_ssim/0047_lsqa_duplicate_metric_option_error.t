#!/bin/sh
# Verify parse error when -m is specified more than once.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_duplicate_metric.err"

run_lsqa -m MS-SSIM -m GMSD "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "metric already specified" "${err_file}" >/dev/null; then
    pass 1 "duplicate -m option was rejected"
else
    fail 1 "duplicate -m option was not rejected as expected"
fi

exit 0
