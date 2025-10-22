#!/usr/bin/env bash
# Validate stdin error handling for img2sixel.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test2] STDIN handling'

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
