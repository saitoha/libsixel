#!/usr/bin/env bash
# Ensure progressive JPEG support works.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo
echo '[test8] progressive jpeg'

require_file "${IMAGES_DIR}/snake-progressive.jpg"

# Convert a progressive JPEG end-to-end.
run_img2sixel "${IMAGES_DIR}/snake-progressive.jpg"
