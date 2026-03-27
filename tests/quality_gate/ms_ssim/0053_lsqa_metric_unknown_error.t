#!/bin/sh
# Verify parse error when -m specifies an unknown metric name.

set -eux


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_metric_unknown.err"

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -m UNKNOWN "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown -m metric was not rejected as expected"
    exit 0
}

grep -q "Unknown metric name" "${err_file}" || {
    echo "not ok" 1 - "unknown -m metric was not rejected as expected"
    exit 0
}

echo "ok" 1 - "unknown -m metric was rejected"

exit 0
