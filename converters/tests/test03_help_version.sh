#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test3] print information\n'

img2sixel -H >/dev/null
img2sixel -V >/dev/null
