#!/bin/sh
set -eu

. "$(dirname "$0")/lib.sh"

require_img2sixel

printf '[test4] conversion options\n'

snake_image=$TOP_SRC_DIR/images/snake.jpg
snake_png=$TOP_SRC_DIR/images/snake.png
snake_gif=$TOP_SRC_DIR/images/snake.gif
snake_tga=$TOP_SRC_DIR/images/snake.tga
snake_tiff=$TOP_SRC_DIR/images/snake.tiff
snake_pgm=$TOP_SRC_DIR/images/snake.pgm
snake_ppm=$TOP_SRC_DIR/images/snake.ppm
snake_palette=$TOP_SRC_DIR/images/snake-palette.png
snake_pbm=$TOP_SRC_DIR/images/snake.pbm
snake_gray_png=$TOP_SRC_DIR/images/snake-grayscale.png
snake_gray_jpg=$TOP_SRC_DIR/images/snake-grayscale.jpg
snake_six=$TOP_SRC_DIR/images/snake.six
snake_bmp=$TOP_SRC_DIR/images/snake.bmp
map8_palette=$TOP_SRC_DIR/images/map8-palette.png
map16_palette=$TOP_SRC_DIR/images/map16-palette.png
map8_six=$TOP_SRC_DIR/images/map8.six
snake_ascii_ppm=$TOP_SRC_DIR/images/snake-ascii.ppm
snake_ascii_pgm=$TOP_SRC_DIR/images/snake-ascii.pgm
snake_ascii_pbm=$TOP_SRC_DIR/images/snake-ascii.pbm
egret_jpg=$TOP_SRC_DIR/images/egret.jpg
map8_png=$TOP_SRC_DIR/images/map8.png
map16_png=$TOP_SRC_DIR/images/map16.png

img2sixel "$snake_image" -datkinson -flum -saverage | \
  img2sixel | tee "$TMP_DIR/snake.sixel" >/dev/null
img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram < "$snake_image" | \
  tee "$TMP_DIR/snake2.sixel" >/dev/null

expected_hex=302131327e2d2131327e1b5c
dcs_payload() {
  printf '\033Pq"1;1;1;1!6~\033\\'
}

actual=$(dcs_payload | img2sixel -r nearest -w200% | tr '#' '\n' | \
  tail -n +3 | od -An -tx1 | tr -d ' \n')
[ "$actual" = "$expected_hex" ] || fail "unexpected hex output (-w200%)"
actual=$(dcs_payload | img2sixel -r nearest -h200% | tr '#' '\n' | \
  tail -n +3 | od -An -tx1 | tr -d ' \n')
[ "$actual" = "$expected_hex" ] || fail "unexpected hex output (-h200%)"
actual=$(dcs_payload | img2sixel -r nearest -h200% -wauto | tr '#' '\n' | \
  tail -n +3 | od -An -tx1 | tr -d ' \n')
[ "$actual" = "$expected_hex" ] || fail "unexpected hex output (-h200% -wauto)"
actual=$(dcs_payload | img2sixel -r nearest -hauto -w12 | tr '#' '\n' | \
  tail -n +3 | od -An -tx1 | tr -d ' \n')
[ "$actual" = "$expected_hex" ] || fail "unexpected hex output (-hauto -w12)"
actual=$(dcs_payload | img2sixel -r nearest -h12 -w200% | tr '#' '\n' | \
  tail -n +3 | od -An -tx1 | tr -d ' \n')
[ "$actual" = "$expected_hex" ] || fail "unexpected hex output (-h12 -w200%)"

img2sixel -w210 -h210 -djajuni -bxterm256 -o "$TMP_DIR/snake3.sixel" < "$snake_image"
img2sixel --height=100 --diffusion=atkinson --outfile="$TMP_DIR/snake4.sixel" < "$snake_image"
img2sixel -w105% -h100 -B#000000000 -rnearest < "$snake_gif" >/dev/null
img2sixel -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto "$snake_tga" >/dev/null
img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning -dstucki -thls "$snake_tiff" >/dev/null
img2sixel -8 -qauto -thls -e "$snake_pgm" >/dev/null
img2sixel -8 -m "$map8_palette" -Esize "$snake_ppm" >/dev/null
img2sixel -7 -m "$map16_palette" -Efast "$snake_image" >/dev/null
img2sixel -7 -w300 "$snake_palette" >/dev/null
img2sixel -7 -w100 -h100 -bxterm16 -B#aB3 -B#aB3 "$snake_pbm" >/dev/null
img2sixel -I -dstucki -thls -B#a0B030 "$snake_ppm" >/dev/null
img2sixel -bvt340color "$snake_ppm" >/dev/null
img2sixel -bvt340mono "$snake_tga" >/dev/null
img2sixel -bgray1 -w120 "$snake_tga" >/dev/null
img2sixel -bgray2 -w120 "$snake_tga" >/dev/null
img2sixel -bgray4 -w120 "$snake_tga" >/dev/null
img2sixel -bgray8 -w120 "$snake_tga" >/dev/null
img2sixel -I -8 -dburkes -B#ffffffffffff "$snake_ascii_ppm" >/dev/null
img2sixel -I -C10 -djajuni "$snake_png" >/dev/null
img2sixel -I -Eauto "$snake_ascii_pgm" >/dev/null
img2sixel -I -datkinson "$snake_ascii_pbm" >/dev/null
img2sixel "$snake_gray_png" >/dev/null
img2sixel -m "$map8_palette" "$snake_gray_png" >/dev/null
img2sixel -m "$snake_gray_png" "$snake_png" >/dev/null
img2sixel -c200x200+100+100 -dx_dither "$snake_gray_png" >/dev/null
img2sixel -c200x200+100+100 -w400 -da_dither "$snake_gray_png" >/dev/null
img2sixel -I "$snake_gray_png" >/dev/null
img2sixel -I "$snake_gray_jpg" >/dev/null
img2sixel -m "$map8_six" -m "$map8_six" "$snake_six" >/dev/null
img2sixel -w200 -p8 "$snake_six" >/dev/null
img2sixel -c200x200+2000+2000 "$snake_six" >/dev/null
img2sixel -bxterm16 "$snake_six" >/dev/null
img2sixel -e "$snake_six" >/dev/null
img2sixel -I "$snake_six" >/dev/null
img2sixel -I -da_dither -w100 "$snake_six" >/dev/null
img2sixel -I -dx_dither -h100 "$snake_six" >/dev/null
img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb "$snake_ppm" >/dev/null
img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls "$snake_bmp" >/dev/null
img2sixel -m "$map8_png" -w200 -fauto -rwelsh "$egret_jpg" >/dev/null
img2sixel -m "$map16_png" -w100 -hauto -rbicubic -dauto "$snake_ppm" >/dev/null
img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "$snake_image" >/dev/null
img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone "$snake_image" >/dev/null
img2sixel -p 2 -h100 -wauto -rlanczos3 "$snake_image" >/dev/null
img2sixel -p 1 -h100 -n1 "$snake_image" >/dev/null && printf '\033[*1z'
img2sixel -e -h140 -rlanczos4 -P "$snake_image" >/dev/null
img2sixel -e -i -P "$snake_image" >/dev/null
img2sixel -w204 -h204 "$snake_png" | img2sixel >/dev/null
