#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test7] animation\n'

img2sixel -ldisable -dnone -u -lauto \
  "$TOP_SRC_DIR/images/seq2gif.gif" >/dev/null
img2sixel -ldisable -dnone -g \
  "$TOP_SRC_DIR/images/seq2gif.gif" >/dev/null
img2sixel -ldisable -dnone -u -g \
  "$TOP_SRC_DIR/images/seq2gif.gif" >/dev/null
img2sixel -S -datkinson "$TOP_SRC_DIR/images/seq2gif.gif" >/dev/null
