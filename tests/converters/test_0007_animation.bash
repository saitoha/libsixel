#!/usr/bin/env bash
# Check animation related switches.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test7] animation'

    require_file "${IMAGES_DIR}/seq2gif.gif"

    # Render GIF while disabling animation playback and enforcing update mode.
    run_img2sixel -ldisable -dnone -u -lauto \
        "${IMAGES_DIR}/seq2gif.gif"
    # Render GIF while forcing animation to a static frame.
    run_img2sixel -ldisable -dnone -g "${IMAGES_DIR}/seq2gif.gif"
    # Render GIF with both update and static frame options combined.
    run_img2sixel -ldisable -dnone -u -g "${IMAGES_DIR}/seq2gif.gif"
    # Render GIF while enabling sequence splitting with Atkinson diffusion.
    run_img2sixel -S -datkinson "${IMAGES_DIR}/seq2gif.gif"
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'animation switches operate'
else
    tap_not_ok 1 'animation switches operate' \
        "See $(tap_log_hint) for details."
fi
