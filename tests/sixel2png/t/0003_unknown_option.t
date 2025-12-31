#!/bin/sh
# TAP test verifying sixel2png rejects unknown options gracefully.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/unknown-option.log"
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

stderr_capture="${output_dir}/stderr.txt"
if run_sixel2png --unknown 2>"${stderr_capture}" >"${output_dir}/stdout.txt"; then
    fail 1 "unknown option should fail"
else
    if grep -qi -- "unrecognized option" "${stderr_capture}"; then
        pass 1 "unknown option reported"
    else
        fail 1 "error message did not mention unknown option"
    fi
fi

exit "${status}"