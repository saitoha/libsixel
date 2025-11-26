#!/usr/bin/env bash
# Exercise a wide range of img2sixel conversion options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test4] conversion options'

snake_jpg="${IMAGES_DIR}/snake.jpg"
snake_png="${IMAGES_DIR}/snake.png"
snake_gif="${IMAGES_DIR}/snake.gif"
snake_tga="${IMAGES_DIR}/snake.tga"
snake_tiff="${IMAGES_DIR}/snake.tiff"
snake_pgm="${IMAGES_DIR}/snake.pgm"
snake_ppm="${IMAGES_DIR}/snake.ppm"
snake_ascii_ppm="${IMAGES_DIR}/snake-ascii.ppm"
snake_ascii_pgm="${IMAGES_DIR}/snake-ascii.pgm"
snake_ascii_pbm="${IMAGES_DIR}/snake-ascii.pbm"
snake_gray_png="${IMAGES_DIR}/snake-grayscale.png"
snake_gray_jpg="${IMAGES_DIR}/snake-grayscale.jpg"
snake_six="${IMAGES_DIR}/snake.six"
map8_palette="${IMAGES_DIR}/map8-palette.png"
map16_palette="${IMAGES_DIR}/map16-palette.png"
map8_six="${IMAGES_DIR}/map8.six"
snake_palette_png="${IMAGES_DIR}/snake-palette.png"
egret_jpg="${IMAGES_DIR}/egret.jpg"

require_file "${snake_jpg}"
require_file "${snake_png}"
require_file "${snake_six}"

# Confirm inline PNG targets accept prefixed output paths.
run_img2sixel -o "png:snake-prefix.png" "${snake_jpg}"
test -s "snake-prefix.png"
# Confirm prefixed PNG targets create missing parent directories.
run_img2sixel -o "png:snake.png" \
    "${snake_jpg}"
test -s "snake.png"
# Confirm plain file names ending in .png trigger PNG output.
run_img2sixel -o "${TMP_DIR}/snake-filename.png" "${snake_jpg}"
od -An -tx1 -N8 "${TMP_DIR}/snake-filename.png" | \
    tr -d ' \n' | \
    xargs test 89504e470d0a1a0a =
# Confirm long option forms operate correctly.
run_img2sixel --height=100 --diffusion=atkinson --outfile="${TMP_DIR}/snake4.sixel" < "${snake_jpg}"
# Convert GIF using precise scaling, background, and filter settings.
run_img2sixel -w105% -h100 -B'#000000000' -rne < "${snake_gif}"
# Check automatic scaling and Gaussian filter on TGA input.
run_img2sixel -7 -sauto -w100 -rga -qauto -dburkes -tauto "${snake_tga}"
# Convert TIFF with palette sizing, centering, and custom colour math.
run_img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki -thls "${snake_tiff}" -o/dev/null
# Ensure palette auto selection with encode flag on PGM input.
run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null
