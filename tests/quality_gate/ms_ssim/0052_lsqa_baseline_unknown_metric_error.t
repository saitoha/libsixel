#!/bin/sh
# Verify parse error when baseline metric name is unknown.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_unknown_metric.err"

set +e
run_lsqa -b "UNKNOWN:0.1" "${image_ref}" >/dev/null 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown baseline metric was not rejected as expected"
    exit 0
}

grep -q "Unknown metric name" "${err_file}" || {
    echo "not ok" 1 - "unknown baseline metric was not rejected as expected"
    exit 0
}

echo "ok" 1 - "unknown baseline metric was rejected"

exit 0
