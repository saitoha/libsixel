#!/usr/bin/env bash
# Validate img2sixel DCS parsing edge cases.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test6] DCS format variations'

    require_file "${IMAGES_DIR}/snake.png"

    # Validate handling of tab-separated colour introducers.
    run_img2sixel "${IMAGES_DIR}/snake.png" | \
        sed 's/C/C:/g' | tr ':' '\t' | \
        run_img2sixel >/dev/null
    # Validate oversized geometry parameters inside the DCS header.
    run_img2sixel "${IMAGES_DIR}/snake.png" | \
        sed 's/"1;1;600;450/"1;1;700;500/' | \
        run_img2sixel >/dev/null
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'DCS parsing tolerates variations'
else
    tap_not_ok 1 'DCS parsing tolerates variations' \
        "See $(tap_log_hint) for details."
fi
