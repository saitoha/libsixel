#!/bin/sh
# Verify parse error when baseline metric name is empty.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_empty_metric.err"

run_lsqa -b ":0.1" "${image_ref}" "${image_out}" \
    >"/dev/null" 2>"${err_file}" || status=$?

if [ "${status-0}" -eq 2 ] &&
        grep -F "Metric name is empty" "${err_file}" >/dev/null; then
    pass 1 "empty baseline metric name was rejected"
else
    fail 1 "empty baseline metric name was not rejected as expected"
fi

exit 0
