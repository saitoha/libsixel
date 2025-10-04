#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_sixel2png
require_img2sixel

printf '[test11] sixel2png\n'

snake_sixel=$TMP_DIR/snake.sixel
snake2_sixel=$TMP_DIR/snake2.sixel
snake3_sixel=$TMP_DIR/snake3.sixel

img2sixel "$TOP_SRC_DIR/images/snake.jpg" -datkinson -flum -saverage | \
  tee "$snake_sixel" >/dev/null
img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram \
  <"$TOP_SRC_DIR/images/snake.jpg" | tee "$snake2_sixel" >/dev/null
img2sixel -w210 -h210 -djajuni -bxterm256 -o "$snake3_sixel" \
  <"$TOP_SRC_DIR/images/snake.jpg"

expect_failure run_sixel2png -i "$TMP_DIR/unknown.sixel"
expect_failure run_sixel2png '-%' <"$snake_sixel"
expect_failure run_sixel2png invalid_filename <"$snake_sixel"

sixel2png -H >/dev/null
sixel2png -V >/dev/null
sixel2png <"$snake_sixel" >"$TMP_DIR/snake1.png"
sixel2png <"$snake2_sixel" >"$TMP_DIR/snake2.png"
sixel2png - - <"$snake3_sixel" >"$TMP_DIR/snake3.png"
sixel2png -i "$snake_sixel" -o "$TMP_DIR/snake4.png"
