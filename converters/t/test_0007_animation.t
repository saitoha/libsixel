#!/usr/bin/env bash
# Check animation related switches.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo
echo '[test7] animation'

require_file "${IMAGES_DIR}/seq2gif.gif"

# Render GIF while disabling animation playback and enforcing update mode.
run_img2sixel -ldisable -dnone -u -lauto "${IMAGES_DIR}/seq2gif.gif"
# Render GIF while forcing animation to a static frame.
run_img2sixel -ldisable -dnone -g "${IMAGES_DIR}/seq2gif.gif"
# Render GIF with both update and static frame options combined.
run_img2sixel -ldisable -dnone -u -g "${IMAGES_DIR}/seq2gif.gif"
# Render GIF while enabling sequence splitting with Atkinson diffusion.
run_img2sixel -S -datkinson "${IMAGES_DIR}/seq2gif.gif"
