#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/version.log"
output_dir="${artifact_dir}/out"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..1"
set -v

version_output="${output_dir}/version.txt"
if run_sixel2png -V >"${version_output}" 2>>"${log_file}"; then
    if grep -Eq '^sixel2png ' "${version_output}"; then
        pass 1 "-V prints version"
    else
        fail 1 "version header missing"
    fi
else
    fail 1 "-V exited with failure"
fi

exit "${status}"
