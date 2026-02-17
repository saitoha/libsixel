#!/bin/sh
# Verify parse error when -m specifies an unknown metric name.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_metric_unknown.err"

set +e
run_lsqa -m UNKNOWN "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    fail 1 "unknown -m metric was not rejected as expected"
    exit 0
}

grep -q "Unknown metric name" "${err_file}" || {
    fail 1 "unknown -m metric was not rejected as expected"
    exit 0
}

pass 1 "unknown -m metric was rejected"

exit 0
