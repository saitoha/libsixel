#!/usr/bin/env bash
# Validate stdin error handling for img2sixel.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +----------------------+---------------------------+
#  | Case                 | Expectation                |
#  +----------------------+---------------------------+
#  | STDIN guard          | Non-image data is rejected |
#  +----------------------+---------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"
tap_plan 1

stdin_rejects_non_image() {
    local output_file

    tap_log "[stdin] invalid stdin should be rejected"
    output_file="${TMP_DIR}/capture.$$"
    if echo -n a | run_img2sixel >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output for invalid stdin\n'
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

tap_case 'STDIN rejects non-image data' stdin_rejects_non_image
