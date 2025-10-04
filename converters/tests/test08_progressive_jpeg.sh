#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test8] progressive jpeg\n'

img2sixel "$TOP_SRC_DIR/images/snake-progressive.jpg" >/dev/null
