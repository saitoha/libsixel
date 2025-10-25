#!/usr/bin/env bash
# Ensure progressive JPEG support works.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +-------------------------------+
#  | Case                          |
#  +-------------------------------+
#  | Progressive JPEG conversion   |
#  +-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

target_image="${IMAGES_DIR}/snake-progressive.jpg"
require_file "${target_image}"

tap_plan 1

progressive_jpeg_converts() {
    tap_log "[progressive-jpeg] converting ${target_image}"
    run_img2sixel "${target_image}"
}

tap_case 'progressive JPEG converts successfully' progressive_jpeg_converts
