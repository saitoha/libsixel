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

run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" -o/dev/null
# Verify fast encoder path with external 16-colour palette.
run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}"
# Confirm large width scaling on indexed PNG input.
run_img2sixel -7 -w300 "${snake_palette_png}"
# Confirm repeated background overrides under terminal palette mode.
run_img2sixel -7 -w100 -h100 -bxterm16 -B'#aB3' -B'#aB3' "${IMAGES_DIR}/snake.pbm"
# Inspect mode with Stucki dithering and custom background.
run_img2sixel -I -dstucki -thls -B'#a0B030' "${snake_ppm}"
# Emit VT340 colour control sequences.
run_img2sixel -bvt340color "${snake_ppm}"
# Emit VT340 monochrome control sequences.
run_img2sixel -bvt340mono "${snake_tga}"
# Emit 1-bit grayscale output.
run_img2sixel -bgray1 -w120 "${snake_tga}"
# Emit 2-bit grayscale output.
run_img2sixel -bgray2 -w120 "${snake_tga}"
# Emit 4-bit grayscale output.
run_img2sixel -bgray4 -w120 "${snake_tga}"
# Emit 8-bit grayscale output.
run_img2sixel -bgray8 -w120 "${snake_tga}"
# Inspect ASCII PPM with 8-bit output and white background.
run_img2sixel -I -8 -dburkes -B'#ffffffffffff' "${snake_ascii_ppm}" > /dev/null
# Inspect PNG while forcing specific colour space and diffusion.
run_img2sixel -I -C10 -djajuni "${snake_png}"
# Inspect ASCII PGM with automatic encoder tuning.
run_img2sixel -I -Eauto "${snake_ascii_pgm}"
# Inspect ASCII PBM with Atkinson diffusion.
run_img2sixel -I -datkinson "${snake_ascii_pbm}"
# Convert grayscale PNG with defaults.
run_img2sixel "${snake_gray_png}"
# Convert grayscale PNG with external palette file.
run_img2sixel -m "${map8_palette}" "${snake_gray_png}"
# Use grayscale PNG as palette for colour PNG conversion.
run_img2sixel -m "${snake_gray_png}" "${snake_png}"
# Crop grayscale PNG before applying X ordered dither.
run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}"
# Crop, scale, and apply alternate ordered dither.
run_img2sixel -c200x200+100+100 -w400 -da_dither "${snake_gray_png}"
# Inspect grayscale PNG metadata without conversion.
run_img2sixel -I "${snake_gray_png}"
# Inspect grayscale JPEG metadata.
run_img2sixel -I "${snake_gray_jpg}"
# Stack palette files to confirm repeated -m handling.
run_img2sixel -m "${map8_six}" -m "${map8_six}" "${snake_six}"
# Resize and limit palette on Sixel input.
run_img2sixel -w200 -p8 "${snake_six}"
