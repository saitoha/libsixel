#!/usr/bin/env bash
# Verify assessment output spooling survives multiple inputs.
#
#   +-----------+    +-----------------+
#   | input #1  |--->|                 |
#   +-----------+    |                 |    +----------+
#                    | Shared encoder  |--->|  stdout  |
#   +-----------+    |                 |    +----------+
#   | input #2  |--->|                 |
#   +-----------+    +-----------------+
#
# The drawing sketches how img2sixel reuses a single encoder instance while
# processing several inputs when the assessment option is active.  A regression
# once left the cached spool path dangling after the first conversion, causing
# the second run to fail when the temporary file disappeared.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

quality_err="${TMP_DIR}/assessment-quality.err"
quality_out="${TMP_DIR}/assessment-quality.out"

require_file "${IMAGES_DIR}/snake.jpg"
require_file "${IMAGES_DIR}/map8.png"

rm -f "${quality_err}" "${quality_out}"

echo '[test1] maintain assessment spool between consecutive inputs'

if ! run_img2sixel -a quality "${IMAGES_DIR}/snake.jpg" \
        "${IMAGES_DIR}/map8.png" \
        >"${quality_out}" 2>"${quality_err}"; then
    echo 'img2sixel failed to process multiple assessment inputs' >&2
    cat "${quality_err}" >&2 || :
    rm -f "${quality_err}" "${quality_out}"
    exit 1
fi

if [[ ! -s "${quality_out}" ]]; then
    echo 'img2sixel did not emit assessment output' >&2
    cat "${quality_err}" >&2 || :
    rm -f "${quality_err}" "${quality_out}"
    exit 1
fi

rm -f "${quality_err}" "${quality_out}"
