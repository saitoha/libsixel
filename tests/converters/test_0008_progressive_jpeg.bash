#!/usr/bin/env bash
# Ensure progressive JPEG support works.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test8] progressive jpeg'

    require_file "${IMAGES_DIR}/snake-progressive.jpg"

    # Convert a progressive JPEG end-to-end.
    run_img2sixel "${IMAGES_DIR}/snake-progressive.jpg"
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'progressive JPEG converts successfully'
else
    tap_not_ok 1 'progressive JPEG converts successfully' \
        "See $(tap_log_hint) for details."
fi
