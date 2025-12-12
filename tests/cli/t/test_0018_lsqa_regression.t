#!/bin/sh
# TAP harness for lsqa_regression.sh so CI surfaces quality regressions.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/lsqa-regression.log"

mkdir -p "${artifact_dir}"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
}

skip() {
    printf 'ok %s # skip - %s\n' "$1" "$2"
}

echo "1..1"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
root_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

# Locate a usable Python interpreter; skip when neither python nor python3
# exists so platforms without scripting support do not fail the suite.
python_bin=${PYTHON:-python}
if ! command -v "${python_bin}" >/dev/null 2>&1; then
    python_bin="python3"
fi

if ! command -v "${python_bin}" >/dev/null 2>&1; then
    skip 1 "python interpreter unavailable"
    exit 0
fi

if "${root_dir}/lsqa_regression.sh" >"${log_file}" 2>&1; then
    pass 1 "lsqa quality baseline held"
else
    fail 1 "lsqa quality regression detected"
    printf '%s\n' '--- log ---' >>"${log_file}"
fi

exit 0
