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

# Crop Sixel input with large offsets.
run_img2sixel -c200x200+2000+2000 "${snake_six}"
# Re-encode Sixel using xterm16 preset.
run_img2sixel -bxterm16 "${snake_six}"
# Encode Sixel input directly to stdout.
run_img2sixel -e "${snake_six}"
# Inspect Sixel metadata.
run_img2sixel -I "${snake_six}"
# Inspect Sixel while forcing alternate ordered dither.
run_img2sixel -I -da_dither -w100 "${snake_six}"
# Inspect Sixel while forcing X ordered dither.
run_img2sixel -I -dx_dither -h100 "${snake_six}"
# Inspect PPM with aggressive cropping, scaling, and filtering tweaks.
run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb "${snake_ppm}"
# Inspect BMP with verbose logging and varied scaling controls.
run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls "${IMAGES_DIR}/snake.bmp"
# Convert JPEG using PNG palette, auto format, and Welsh filter.
run_img2sixel -m "${IMAGES_DIR}/map8.png" -w200 -fau -rwelsh "${egret_jpg}"
# Convert PPM using 16-colour palette with bicubic scaling.
run_img2sixel -m "${IMAGES_DIR}/map16.png" -w100 -hauto -rbicubic -dauto "${snake_ppm}"
# Limit palette and adjust colour space for Lanczos2 scaling.
run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}"
# Enable verbose logging while testing Lanczos2 without diffusion.
run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone "${snake_jpg}"
# Reduce palette to two colours with Lanczos3 scaling.
run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}"
# Verify monochrome output and single-frame animation trailer.
run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" && printf '\033[*1z'
# Encode with Lanczos4 scaling while dumping palette.
run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}"
# Encode with interlace enabled while dumping palette.
run_img2sixel -e -i -P "${snake_jpg}" >/dev/null
# Re-encode scaled PNG output through a second conversion pass.
run_img2sixel -w204 -h204 "${snake_png}" | run_img2sixel >/dev/null
