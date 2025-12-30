#!/bin/sh
# TAP test verifying sixel2png reports version/help and exits successfully.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/version-help.log"
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

echo "1..2"

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

help_output="${output_dir}/help.txt"
if run_sixel2png -H 2>>"${log_file}" 1>"${help_output}"; then
    if grep -Eq '^Usage: sixel2png' "${help_output}"; then
        pass 2 "-H prints usage"
    else
        fail 2 "help usage header missing"
    fi
else
    fail 2 "-H exited with failure"
fi

exit "${status}"
