#!/bin/sh
# Verify parse error when baseline metric name is unknown.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_unknown_metric.err"

run_lsqa -b "UNKNOWN:0.1" "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "Unknown metric name" "${err_file}" >/dev/null; then
    pass 1 "unknown baseline metric was rejected"
else
    fail 1 "unknown baseline metric was not rejected as expected"
fi

exit 0
