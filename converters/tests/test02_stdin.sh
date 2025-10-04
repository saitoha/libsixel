#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test2] STDIN handling\n'

if echo -n a | img2sixel >/dev/null 2>&1; then
  fail 'expected img2sixel to reject raw stdin data'
fi
