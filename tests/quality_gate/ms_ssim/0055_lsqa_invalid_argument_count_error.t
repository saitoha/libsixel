#!/bin/sh
# Verify parse error when lsqa receives too many positional arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
extra_arg="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_invalid_arg_count.err"

set +e
run_lsqa "${image_ref}" "${image_out}" "${extra_arg}" >/dev/null 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "invalid positional argument count was not rejected as expected"
    exit 0
}

grep "invalid number of arguments" "${err_file}" >/dev/null || {
    echo "not ok" 1 - "invalid positional argument count was not rejected as expected"
    exit 0
}

echo "ok" 1 - "invalid positional argument count was rejected"
exit 0
