#!/usr/bin/env bash
# Validate img2sixel DCS parsing edge cases.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo
echo '[test6] DCS format variations'

require_file "${IMAGES_DIR}/snake.png"

# Validate handling of tab-separated colour introducers.
run_img2sixel "${IMAGES_DIR}/snake.png" | \
    sed 's/C/C:/g' | tr ':' '\t' | \
    run_img2sixel >/dev/null
# Validate oversized geometry parameters inside the DCS header.
run_img2sixel "${IMAGES_DIR}/snake.png" | \
    sed 's/"1;1;600;450/"1;1;700;500/' | \
    run_img2sixel >/dev/null
