#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test6] DCS format variations\n'

img2sixel "$TOP_SRC_DIR/images/snake.png" | \
  sed 's/C/C:/g' | tr : '\t' | \
  img2sixel >/dev/null
img2sixel "$TOP_SRC_DIR/images/snake.png" | \
  sed 's/"1;1;600;450/"1;1;700;500/' | \
  img2sixel >/dev/null
