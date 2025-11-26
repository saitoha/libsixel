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

# Validate dithering, gamma, and scaling through a round-trip convert.
run_img2sixel "${snake_jpg}" -datkinson -flum -save | \
    run_img2sixel | tee "${TMP_DIR}/snake.sixel" >/dev/null
# Reject ambiguous select-color prefixes.
if run_img2sixel -sa "${snake_jpg}" >/dev/null 2>&1; then
    echo "expected -sa to fail due to ambiguous prefix" >&2
    exit 1
fi
# Check unique select-color prefixes.
run_img2sixel -shist "${snake_jpg}" >/dev/null
# Check percentage scaling, histogram stats, and background overrides.
run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist < "${snake_jpg}" | \
    tee "${TMP_DIR}/snake2.sixel" >/dev/null
# Ensure width scaling preserves expected DCS coordinates.

printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -rne -w200% | \
    tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
    xargs test 302131327e2d2131327e1b5c =
# Ensure height scaling preserves expected DCS coordinates.
printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -rne -h200% | \
    tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
    xargs test 302131327e2d2131327e1b5c =
# Ensure automatic width cooperates with explicit height scaling.
printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -rne -h200% -wauto | \
    tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
    xargs test 302131327e2d2131327e1b5c =
# Ensure automatic height cooperates with explicit width scaling.
printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -rne -hauto -w12 | \
    tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
    xargs test 302131327e2d2131327e1b5c =
# Ensure combined absolute and percentage scaling stays consistent.
printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -rne -h12 -w200% | \
    tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
    xargs test 302131327e2d2131327e1b5c =
# Exercise explicit dimensions, dithering, and terminal palette output.
run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${TMP_DIR}/snake3.sixel" < "${snake_jpg}"
