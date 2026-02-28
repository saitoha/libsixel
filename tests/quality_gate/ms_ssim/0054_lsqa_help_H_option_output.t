#!/bin/sh
# Verify -H prints help text to stdout and exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

out_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stdout"

run_lsqa -H >"${out_file}" || {
    echo "not ok" 1 "lsqa -H should exit with success"
    exit 0
}

awk '/Usage: lsqa/ { m++ } /Options:/ { m++ } END { if (! m) exit 1 }' "${out_file}" || {
    echo "not ok" 1 "lsqa -H did not print expected help output"
    exit 0
}

echo "ok" 1 "lsqa -H printed help text"
exit 0
