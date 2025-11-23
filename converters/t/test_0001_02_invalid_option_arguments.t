#!/usr/bin/env bash
# Verify error handling for img2sixel options with invalid arguments.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test2] invalid option argument handling'

# Ensure reference inputs exist for option validation checks that need them.
for name in map8.png snake.jpg snake.png; do
    require_file "${IMAGES_DIR}/${name}"
done

expect_failure() {
    local output_file
    local label

    output_file=$(mktemp "${TMP_DIR}/capture.invalid.XXXXXX")
    label="$*"
    # Windows builds may fall back to stdin when an input path is rejected.
    # Redirect /dev/null so the executable cannot block while waiting for
    # console input once argument validation fails.
    if run_img2sixel "$@" </dev/null >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' \
            "${label}" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
}

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
expect_failure -B '#ffff' "${IMAGES_DIR}/map8.png"
# Reject an overly long background colour specification.
expect_failure -B '#0000000000000' "${IMAGES_DIR}/map8.png"
# Reject a malformed hex colour.
expect_failure -B '#00G'
# Reject an unknown named colour.
expect_failure -B test
# Reject an incomplete rgb: colour form.
expect_failure -B 'rgb:11/11'
# Reject the unsupported legacy width syntax.
expect_failure '-%'
# Reject a palette file that does not exist.
expect_failure -m "${TMP_DIR}/invalid_filename" "${IMAGES_DIR}/snake.jpg"
# Reject an invalid colour space index.
expect_failure -I -C0 "${IMAGES_DIR}/snake.png"
