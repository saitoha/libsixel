#!/usr/bin/env bash
# Validate sixel2png behaviour.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test11] sixel2png'

for name in snake.sixel snake2.sixel snake3.sixel; do
    require_file "${TMP_DIR}/${name}"
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

expect_failure -i "${TMP_DIR}/unknown.sixel"
output_file="${TMP_DIR}/capture.$$"
if run_sixel2png -% < "${TMP_DIR}/snake.sixel" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for -%' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
output_file="${TMP_DIR}/capture.$$"
if run_sixel2png invalid_filename < "${TMP_DIR}/snake.sixel" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'sixel2png unexpectedly produced output for invalid filename' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"

run_sixel2png -H
run_sixel2png -V
run_sixel2png < "${TMP_DIR}/snake.sixel" > "${TMP_DIR}/snake1.png"
run_sixel2png < "${TMP_DIR}/snake2.sixel" > "${TMP_DIR}/snake2.png"
run_sixel2png - - < "${TMP_DIR}/snake3.sixel" > "${TMP_DIR}/snake3.png"
run_sixel2png -i "${TMP_DIR}/snake.sixel" -o "${TMP_DIR}/snake4.png"
