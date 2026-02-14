#!/bin/sh
# Verify -H prints help text to stdout and exits successfully.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

out_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stdout"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stderr"

run_lsqa -H >"${out_file}" 2>"${err_file}" || {
    fail 1 "lsqa -H should exit with success"
    exit 0
}

grep "Usage: lsqa" "${out_file}" >/dev/null || {
    fail 1 "lsqa -H did not print expected help output"
    exit 0
}

grep "Options:" "${out_file}" >/dev/null || {
    fail 1 "lsqa -H did not print expected help output"
    exit 0
}

pass 1 "lsqa -H printed help text"

exit 0
