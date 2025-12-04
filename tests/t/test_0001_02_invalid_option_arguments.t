#!/bin/sh
# TAP test verifying img2sixel rejects options with invalid arguments
# and emits no stray output.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/invalid-option-arguments.log"
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

expect_failure() {
    index=$1
    description=$2
    shift 2

    output_file=$(make_temp_file "${tmp_dir}" "capture.invalid")
    # Redirect stdin to /dev/null to avoid blocking if argument parsing falls
    # back to reading from stdin after rejecting the provided options.
    if run_img2sixel "$@" </dev/null >"${output_file}" 2>>"${log_file}"; then
        cmd_status=0
    else
        cmd_status=$?
    fi

    if [ "${cmd_status}" -eq 0 ]; then
        fail "${index}" "unexpected success: ${description}"
    elif [ -s "${output_file}" ]; then
        fail "${index}" "unexpected output: ${description}"
    else
        pass "${index}" "${description}"
    fi

    rm -f "${output_file}"
}

require_file "${images_dir}/map8.png"
require_file "${images_dir}/snake.jpg"
require_file "${images_dir}/snake.png"

echo "1..20"

expect_failure 1 "unknown dither option" -d invalid_option
expect_failure 2 "unknown resize filter" -r invalid_option
expect_failure 3 "unknown scaling mode" -s invalid_option
expect_failure 4 "unknown tone adjustment" -t invalid_option
expect_failure 5 "invalid width value" -w invalid_option
expect_failure 6 "invalid height value" -h invalid_option
expect_failure 7 "invalid format name" -f invalid_option
expect_failure 8 "invalid quality preset" -q invalid_option
expect_failure 9 "invalid layout option" -l invalid_option
expect_failure 10 "invalid bits-per-pixel argument" -b invalid_option
expect_failure 11 "invalid encoder tweak" -E invalid_option
expect_failure 12 "invalid background colour" -B invalid_option
expect_failure 13 "missing background component" -B '#ffff' "${images_dir}/map8.png"
expect_failure 14 "overlong background specification" -B '#0000000000000' "${images_dir}/map8.png"
expect_failure 15 "malformed hex background" -B '#00G'
expect_failure 16 "unknown named colour" -B test
expect_failure 17 "incomplete rgb background" -B 'rgb:11/11'
expect_failure 18 "unsupported legacy width syntax" '-%'
expect_failure 19 "missing palette file" -m "${tmp_dir}/invalid_filename" "${images_dir}/snake.jpg"
expect_failure 20 "invalid colour space index" -I -C0 "${images_dir}/snake.png"

exit "${status}"
