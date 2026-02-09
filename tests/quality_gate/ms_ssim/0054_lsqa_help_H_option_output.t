#!/bin/sh
# Verify -H prints help text to stdout and exits successfully.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

out_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stdout"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stderr"

if run_lsqa -H >"${out_file}" 2>"${err_file}"; then
    :
else
    fail 1 "lsqa -H should exit with success"
    exit 0
fi

if grep -F "Usage: lsqa" "${out_file}" >/dev/null &&
        grep -F "Options:" "${out_file}" >/dev/null; then
    pass 1 "lsqa -H printed help text"
else
    fail 1 "lsqa -H did not print expected help output"
fi

exit 0
