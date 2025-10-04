#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test13] regression test\n'

expect_exit_in "0 127 255" \
  img2sixel -B '#000' -B ''
expect_exit_in "0 127 255" \
  img2sixel "$TOP_SRC_DIR/tests/issue/167/poc" -h128
expect_exit_in "0 127 255" \
  img2sixel "$TOP_SRC_DIR/tests/issue/166/poc" -w128
img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose \
  "$TOP_SRC_DIR/tests/issue/200/POC_img2sixel_heap_buffer_overflow" \
  -o /dev/null
