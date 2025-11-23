#!/usr/bin/env bash
# Verify error handling for incompatible img2sixel option combinations.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test3] invalid option combination handling'

# Ensure reference inputs exist for option validation checks that need them.
for name in snake.jpg snake.png; do
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

# Reject mutually exclusive palette and encode flags.
expect_failure -p16 -e "${IMAGES_DIR}/snake.jpg"
# Reject incompatible inspect and palette options.
expect_failure -I -p8 "${IMAGES_DIR}/snake.png"
# Reject conflicting palette size and terminal preset.
expect_failure -p64 -bxterm256 "${IMAGES_DIR}/snake.png"
# Reject 8-bit output when palette dump is requested.
expect_failure -8 -P "${IMAGES_DIR}/snake.png"
