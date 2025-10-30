#!/usr/bin/env bash
# Verify error handling for invalid img2sixel options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test1] invalid option handling'

# Ensure an unreadable input file does not leave stray output.
invalid_file="${TMP_DIR}/testfile"
rm -f "${invalid_file}"
touch "${invalid_file}"
chmod a-r "${invalid_file}"
# Expect img2sixel to fail cleanly when the source is unreadable.
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

# Reject a missing input path.
expect_failure "${TMP_DIR}/invalid_filename"
# Reject a directory as input.
expect_failure "."
# Report an unknown dither option.
expect_failure -d invalid_option
# Report an unknown resize filter.
expect_failure -r invalid_option
# Report an unknown scaling mode.
expect_failure -s invalid_option
# Report an unknown tone adjustment mode.
expect_failure -t invalid_option
# Report an invalid width value.
expect_failure -w invalid_option
# Report an invalid height value.
expect_failure -h invalid_option
# Report an invalid format name.
expect_failure -f invalid_option
# Report an invalid quality preset.
expect_failure -q invalid_option
# Report an invalid layout option.
expect_failure -l invalid_option
# Report an invalid bits-per-pixel argument.
expect_failure -b invalid_option
# Report an invalid encoder tweak.
expect_failure -E invalid_option
# Report an invalid background colour string.
expect_failure -B invalid_option
# Reject a background colour missing one component.
expect_failure -B '#ffff' "${TOP_SRCDIR}/images/map8.png"
# Reject an overly long background colour specification.
expect_failure -B '#0000000000000' "${TOP_SRCDIR}/images/map8.png"
# Reject a malformed hex colour.
expect_failure -B '#00G'
# Reject an unknown named colour.
expect_failure -B test
# Reject an incomplete rgb: colour form.
expect_failure -B 'rgb:11/11'
# Reject the unsupported legacy width syntax.
expect_failure '-%'
# Reject a palette file that does not exist.
expect_failure -m "${TMP_DIR}/invalid_filename" "${TOP_SRCDIR}/images/snake.jpg"
# Reject mutually exclusive palette and encode flags.
expect_failure -p16 -e "${TOP_SRCDIR}/images/snake.jpg"
# Reject an invalid colour space index.
expect_failure -I -C0 "${TOP_SRCDIR}/images/snake.png"
# Reject incompatible inspect and palette options.
expect_failure -I -p8 "${TOP_SRCDIR}/images/snake.png"
# Reject conflicting palette size and terminal preset.
expect_failure -p64 -bxterm256 "${TOP_SRCDIR}/images/snake.png"
# Reject 8-bit output when palette dump is requested.
expect_failure -8 -P "${TOP_SRCDIR}/images/snake.png"
