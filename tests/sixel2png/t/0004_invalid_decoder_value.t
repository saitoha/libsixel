#!/bin/sh
# TAP test verifying invalid decoder arguments surface descriptive errors.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/invalid-decoder.log"
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

input_path="${images_dir}/snake.six"
require_file "${input_path}"

stderr_capture="${output_dir}/stderr.txt"
if run_sixel2png --similarity=invalid "${input_path}" \
        >"${output_dir}/stdout.txt" 2>"${stderr_capture}"; then
    fail 1 "invalid similarity should fail"
else
    if grep -qi -- "similarity" "${stderr_capture}" \
            || grep -qi -- "SIXEL_BAD_ARGUMENT" "${stderr_capture}"; then
        pass 1 "invalid similarity reported"
    else
        fail 1 "error message missing similarity hint"
    fi
fi

exit "${status}"
