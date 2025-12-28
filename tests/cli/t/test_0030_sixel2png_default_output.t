#!/bin/sh
# TAP test verifying default output naming when -o/--output is omitted.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/default-output.log"
output_dir="${artifact_dir}/out"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

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

input_path="${images_dir}/snake.six"
require_file "${input_path}"

output_path="${output_dir}/snake.png"
rm -f "${output_path}"

if run_sixel2png -i "${input_path}" >"${output_dir}/stdout.txt" \
        2>"${output_dir}/stderr.txt"; then
    if [ -s "${output_path}" ]; then
        pass 1 "default output name created"
    else
        fail 1 "default-named png not created"
    fi
else
    fail 1 "sixel2png without -o failed"
fi

exit "${status}"
