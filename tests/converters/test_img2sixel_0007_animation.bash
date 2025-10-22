#!/usr/bin/env bash
# Check animation related switches.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo
echo '[test7] animation'

require_file "${IMAGES_DIR}/seq2gif.gif"

run_img2sixel -ldisable -dnone -u -lauto "${IMAGES_DIR}/seq2gif.gif"
run_img2sixel -ldisable -dnone -g "${IMAGES_DIR}/seq2gif.gif"
run_img2sixel -ldisable -dnone -u -g "${IMAGES_DIR}/seq2gif.gif"
run_img2sixel -S -datkinson "${IMAGES_DIR}/seq2gif.gif"
