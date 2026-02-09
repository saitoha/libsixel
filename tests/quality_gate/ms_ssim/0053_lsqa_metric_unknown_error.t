#!/bin/sh
# Verify parse error when -m specifies an unknown metric name.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_metric_unknown.err"

run_lsqa -m UNKNOWN "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "Unknown metric name" "${err_file}" >/dev/null; then
    pass 1 "unknown -m metric was rejected"
else
    fail 1 "unknown -m metric was not rejected as expected"
fi

exit 0
