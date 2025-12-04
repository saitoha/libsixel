#!/bin/sh
# TAP test verifying img2sixel rejects invalid positional inputs without
# emitting stray output.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/invalid-positional.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..2"

missing_path="${tmp_dir}/invalid_filename"
rm -f "${missing_path}"
missing_output=$(make_temp_file "${tmp_dir}" "capture.invalid")
if run_img2sixel "${missing_path}" >"${missing_output}" 2>>"${log_file}"; then
    fail 1 "img2sixel accepted missing input"
elif [ -s "${missing_output}" ]; then
    fail 1 "img2sixel produced output for missing input"
else
    pass 1 "missing input rejected without output"
fi
rm -f "${missing_output}" "${missing_path}"

invalid_output=$(make_temp_file "${tmp_dir}" "capture.invalid")
if run_img2sixel "." </dev/null >"${invalid_output}" 2>>"${log_file}"; then
    fail 2 "img2sixel accepted directory input"
elif [ -s "${invalid_output}" ]; then
    fail 2 "img2sixel produced output for directory input"
else
    pass 2 "directory input rejected without output"
fi
rm -f "${invalid_output}"

exit "${status}"
