#!/usr/bin/env bash
# Validate stdin error handling for img2sixel.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test2] STDIN handling'

    # Verify that non-image stdin is rejected.
    output_file="${TMP_DIR}/capture.$$"
    if echo -n a | run_img2sixel >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        echo 'img2sixel unexpectedly produced output for invalid stdin' >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'STDIN rejects non-image data'
else
    tap_not_ok 1 'STDIN rejects non-image data' \
        "See $(tap_log_hint) for details."
fi
