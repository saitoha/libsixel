#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test1] invalid option handling\n'

testfile=$TMP_DIR/testfile
touch "$testfile"
chmod -r "$testfile"
expect_failure img2sixel "$testfile"
rm -f "$testfile"

expect_failure img2sixel "$TMP_DIR/invalid_filename"
expect_failure img2sixel "."
expect_failure img2sixel -d invalid_option
expect_failure img2sixel -r invalid_option
expect_failure img2sixel -s invalid_option
expect_failure img2sixel -t invalid_option
expect_failure img2sixel -w invalid_option
expect_failure img2sixel -h invalid_option
expect_failure img2sixel -f invalid_option
expect_failure img2sixel -q invalid_option
expect_failure img2sixel -l invalid_option
expect_failure img2sixel -b invalid_option
expect_failure img2sixel -E invalid_option
expect_failure img2sixel -B invalid_option
expect_failure img2sixel -B '#ffff' "$TOP_SRC_DIR/images/map8.png"
expect_failure img2sixel -B '#0000000000000' "$TOP_SRC_DIR/images/map8.png"
expect_failure img2sixel -B '#00G'
expect_failure img2sixel -B test
expect_failure img2sixel -B 'rgb:11/11'
expect_failure img2sixel '-%'
expect_failure img2sixel -m "$TMP_DIR/invalid_filename" "$TOP_SRC_DIR/images/snake.jpg"
expect_failure img2sixel -p16 -e "$TOP_SRC_DIR/images/snake.jpg"
expect_failure img2sixel -I -C0 "$TOP_SRC_DIR/images/snake.png"
expect_failure img2sixel -I -p8 "$TOP_SRC_DIR/images/snake.png"
expect_failure img2sixel -p64 -bxterm256 "$TOP_SRC_DIR/images/snake.png"
expect_failure img2sixel -8 -P "$TOP_SRC_DIR/images/snake.png"
