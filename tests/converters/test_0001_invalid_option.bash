#!/usr/bin/env bash
# Verify error handling for invalid img2sixel options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test1] invalid option handling'

invalid_file="${TMP_DIR}/testfile"
rm -f "${invalid_file}"
touch "${invalid_file}"
chmod a-r "${invalid_file}"
output_file="${TMP_DIR}/capture.$$"
if run_img2sixel "${invalid_file}" >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'img2sixel unexpectedly produced output for unreadable file' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
rm -f "${invalid_file}"

rm -f "${TMP_DIR}/invalid_filename"

expect_failure() {
    local output_file

    output_file="${TMP_DIR}/capture.$$"
    if run_img2sixel "$@" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' "$*" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
}

expect_failure "${TMP_DIR}/invalid_filename"
expect_failure "."
expect_failure -d invalid_option
expect_failure -r invalid_option
expect_failure -s invalid_option
expect_failure -t invalid_option
expect_failure -w invalid_option
expect_failure -h invalid_option
expect_failure -f invalid_option
expect_failure -q invalid_option
expect_failure -l invalid_option
expect_failure -b invalid_option
expect_failure -E invalid_option
expect_failure -B invalid_option
expect_failure -B '#ffff' "${TOP_SRCDIR}/images/map8.png"
expect_failure -B '#0000000000000' "${TOP_SRCDIR}/images/map8.png"
expect_failure -B '#00G'
expect_failure -B test
expect_failure -B 'rgb:11/11'
expect_failure '-%'
expect_failure -m "${TMP_DIR}/invalid_filename" "${TOP_SRCDIR}/images/snake.jpg"
expect_failure -p16 -e "${TOP_SRCDIR}/images/snake.jpg"
expect_failure -I -C0 "${TOP_SRCDIR}/images/snake.png"
expect_failure -I -p8 "${TOP_SRCDIR}/images/snake.png"
expect_failure -p64 -bxterm256 "${TOP_SRCDIR}/images/snake.png"
expect_failure -8 -P "${TOP_SRCDIR}/images/snake.png"
