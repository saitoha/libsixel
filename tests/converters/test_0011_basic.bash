#!/usr/bin/env bash
# Validate sixel2png behaviour.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +-----------------------------+-------------------------------+
#  | Category                    | Cases                         |
#  +-----------------------------+-------------------------------+
#  | Failure handling            | missing file, -%, bad output  |
#  | Informational commands      | -H, -V                        |
#  | Conversion paths            | stdin/stdout variations       |
#  +-----------------------------+-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

for name in snake.six map8.six map64.six; do
    require_file "${IMAGES_DIR}/${name}"
done

sixel2png_failure_case() {
    local description
    local command
    local output_file

    description=$1
    shift
    command=("$@")
    output_file="${TMP_DIR}/capture.$$"
    tap_log "[sixel2png-failure] ${description} :: ${command[*]}"
    if "${command[@]}" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'sixel2png unexpectedly produced output: %s\n' \
            "${command[*]}"
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

legacy_width_failure() {
    local output_file

    tap_log '[sixel2png-failure] invalid legacy width syntax'
    cp "${IMAGES_DIR}/snake.six" "${TMP_DIR}/snake.six"
    output_file="${TMP_DIR}/capture.$$"
    if run_sixel2png -% <"${TMP_DIR}/snake.six" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        echo 'sixel2png unexpectedly produced output for -%'
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

invalid_output_failure() {
    local output_file

    tap_log '[sixel2png-failure] invalid output filename'
    output_file="${TMP_DIR}/capture.$$"
    if run_sixel2png invalid_filename <"${IMAGES_DIR}/snake.six" \
            >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        echo 'sixel2png unexpectedly produced output for invalid filename'
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

sixel2png_help() {
    tap_log '[sixel2png-info] -H'
    run_sixel2png -H
}

sixel2png_version() {
    tap_log '[sixel2png-info] -V'
    run_sixel2png -V
}

convert_snake_stdin() {
    tap_log '[sixel2png-convert] snake via stdin'
    run_sixel2png <"${IMAGES_DIR}/snake.six" >"${TMP_DIR}/snake1.png"
}

convert_map8_stdin() {
    tap_log '[sixel2png-convert] map8 via stdin'
    run_sixel2png <"${IMAGES_DIR}/map8.six" >"${TMP_DIR}/map8.png"
}

convert_map64_explicit() {
    tap_log '[sixel2png-convert] map64 via explicit markers'
    run_sixel2png - - <"${IMAGES_DIR}/map64.six" >"${TMP_DIR}/map64.png"
}

convert_snake_files() {
    tap_log '[sixel2png-convert] snake via file arguments'
    run_sixel2png -i "${IMAGES_DIR}/snake.six" -o "${TMP_DIR}/snake4.png"
}

cases=9
tap_plan "${cases}"

tap_case 'sixel2png rejects missing file' sixel2png_failure_case \
    'missing input file' run_sixel2png -i "${TMP_DIR}/unknown.six"
tap_case 'sixel2png ignores legacy -%' legacy_width_failure
tap_case 'sixel2png rejects invalid output path' invalid_output_failure
tap_case 'sixel2png -H works' sixel2png_help
tap_case 'sixel2png -V works' sixel2png_version
tap_case 'sixel2png converts snake via stdin' convert_snake_stdin
tap_case 'sixel2png converts map8 via stdin' convert_map8_stdin
tap_case 'sixel2png converts map64 via markers' convert_map64_explicit
tap_case 'sixel2png converts snake via file arguments' convert_snake_files
