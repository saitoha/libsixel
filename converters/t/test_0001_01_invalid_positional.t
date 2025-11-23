#!/usr/bin/env bash
# Verify error handling for invalid img2sixel positional inputs.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test1] invalid positional input handling'

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

# Ensure an unreadable input file does not leave stray output.
invalid_file="${TMP_DIR}/invalid-input"
rm -f "${invalid_file}"
touch "${invalid_file}"
chmod a-r "${invalid_file}"
expect_failure "${invalid_file}"
rm -f "${invalid_file}"
rm -f "${TMP_DIR}/invalid_filename"

# Reject a missing input path.
expect_failure "${TMP_DIR}/invalid_filename"

# Reject a directory as input.
expect_failure "."
