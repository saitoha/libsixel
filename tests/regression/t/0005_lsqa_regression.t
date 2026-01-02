#!/bin/sh
# TAP harness for lsqa_regression.sh so CI surfaces quality regressions.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/lsqa-regression.log"

mkdir -p "${artifact_dir}"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
}

printf '1..1\n'

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
root_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

if VERBOSE=1 "${root_dir}/lsqa_regression.sh" >"${log_file}" 2>&1; then
    pass 1 "lsqa quality baseline held"
else
    cat "${log_file}" >&2
    fail 1 "lsqa quality regression detected"
fi

exit 0
