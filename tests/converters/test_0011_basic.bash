#!/usr/bin/env bash
# Validate sixel2png behaviour.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test11] sixel2png'

for name in snake.six map8.six map64.six; do
    require_file "${IMAGES_DIR}/${name}"
done

expect_failure() {
    local output_file

    output_file="${TMP_DIR}/capture.$$"
    if run_sixel2png "$@" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'sixel2png unexpectedly produced output: %s\n' "$*" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
}

# Reject nonexistent Sixel input file.
expect_failure -i "${TMP_DIR}/unknown.six"
output_file="${TMP_DIR}/capture.$$"
# Ensure invalid legacy width syntax is ignored.
if run_sixel2png -% < "${TMP_DIR}/snake.six" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for -%' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
output_file="${TMP_DIR}/capture.$$"
# Ensure invalid output filename is rejected.
if run_sixel2png invalid_filename < "${IMAGES_DIR}/snake.six" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for invalid filename' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"

# Confirm help output is accessible.
run_sixel2png -H
# Confirm version output is accessible.
run_sixel2png -V
# Convert Sixel snake to PNG via stdin.
run_sixel2png < "${IMAGES_DIR}/snake.six" > "${TMP_DIR}/snake1.png"
# Convert Sixel map8 to PNG via stdin.
run_sixel2png < "${IMAGES_DIR}/map8.six" > "${TMP_DIR}/map8.png"
# Convert Sixel map64 to PNG using explicit stdin/stdout markers.
run_sixel2png - - < "${IMAGES_DIR}/map64.six" > "${TMP_DIR}/map64.png"
# Convert Sixel snake to PNG using file arguments.
run_sixel2png -i "${IMAGES_DIR}/snake.six" -o "${TMP_DIR}/snake4.png"
