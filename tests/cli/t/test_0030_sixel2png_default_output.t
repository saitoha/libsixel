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

stdout_path="${output_dir}/stdout.png"
stderr_path="${output_dir}/stderr.txt"
rm -f "${stdout_path}" "${stderr_path}"

if run_sixel2png -i "${input_path}" >"${stdout_path}" 2>"${stderr_path}"; then
    if [ -s "${stdout_path}" ] \
            && [ "$(head -c 8 "${stdout_path}" | \
                LC_ALL=C od -An -tx1 | tr -d ' \n')" \
                = "89504e470d0a1a0a" ]; then
        pass 1 "default stdout PNG produced"
    else
        fail 1 "stdout png missing or invalid signature"
    fi
else
    fail 1 "sixel2png without -o failed"
fi

exit "${status}"
