#!/usr/bin/env bash
# Exercise a wide range of img2sixel conversion options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +---------------------------------------------------------------+
#  | A sprawling matrix of img2sixel invocations is represented as |
#  | an ordered list of shell snippets.  Each snippet becomes its   |
#  | own TAP case so that failures can be narrowed down instantly. |
#  +---------------------------------------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

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

declare -a CONVERSION_DESCRIPTIONS=()
declare -a CONVERSION_SNIPPETS=()

register_conversion_case() {
    local description

    description=$1
    shift
    CONVERSION_DESCRIPTIONS+=("${description}")
    CONVERSION_SNIPPETS+=("$*")
}

execute_conversion_case() {
    local snippet

    snippet=$1
    tap_log "[conversion] ${snippet}"
    eval "${snippet}"
}

register_conversion_case 'round-trip conversion with dithering' \
    'run_img2sixel "${snake_jpg}" -datkinson -flum -saverage | \
        run_img2sixel | tee "${TMP_DIR}/snake.sixel" >/dev/null'
register_conversion_case 'percentage scaling with histogram and background' \
    'run_img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram < \
        "${snake_jpg}" | tee "${TMP_DIR}/snake2.sixel" >/dev/null'
register_conversion_case 'width scaling preserves coordinates' \
    'printf "\033Pq\"1;1;1;1!6~\033\\" | run_img2sixel -r nearest -w200% | \
        tr "#" "\n" | tail -n +3 | od -An -tx1 | tr -d " " | \
        xargs test 302131327e2d2131327e1b5c ='
register_conversion_case 'height scaling preserves coordinates' \
    'printf "\033Pq\"1;1;1;1!6~\033\\" | run_img2sixel -r nearest -h200% | \
        tr "#" "\n" | tail -n +3 | od -An -tx1 | tr -d " " | \
        xargs test 302131327e2d2131327e1b5c ='
register_conversion_case 'auto width with explicit height' \
    'printf "\033Pq\"1;1;1;1!6~\033\\" | run_img2sixel -r nearest -h200% -wauto | \
        tr "#" "\n" | tail -n +3 | od -An -tx1 | tr -d " " | \
        xargs test 302131327e2d2131327e1b5c ='
register_conversion_case 'auto height with explicit width' \
    'printf "\033Pq\"1;1;1;1!6~\033\\" | run_img2sixel -r nearest -hauto -w12 | \
        tr "#" "\n" | tail -n +3 | od -An -tx1 | tr -d " " | \
        xargs test 302131327e2d2131327e1b5c ='
register_conversion_case 'mixed absolute and percentage scaling' \
    'printf "\033Pq\"1;1;1;1!6~\033\\" | run_img2sixel -r nearest -h12 -w200% | \
        tr "#" "\n" | tail -n +3 | od -An -tx1 | tr -d " " | \
        xargs test 302131327e2d2131327e1b5c ='
register_conversion_case 'explicit size with terminal palette output' \
    'run_img2sixel -w210 -h210 -djajuni -bxterm256 -o \
        "${TMP_DIR}/snake3.sixel" < "${snake_jpg}"'
register_conversion_case 'long options work' \
    'run_img2sixel --height=100 --diffusion=atkinson \
        --outfile="${TMP_DIR}/snake4.sixel" < "${snake_jpg}"'
register_conversion_case 'gif conversion with custom background' \
    'run_img2sixel -w105% -h100 -B"#000000000" -rnearest < "${snake_gif}"'
register_conversion_case 'tga conversion with gaussian filter' \
    'run_img2sixel -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto \
        "${snake_tga}"'
register_conversion_case 'tiff conversion with palette sizing' \
    'run_img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning \
        -dstucki -thls "${snake_tiff}" -o/dev/null'
register_conversion_case 'pgm conversion with auto encoder' \
    'run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null'
register_conversion_case 'ppm conversion with explicit palette file' \
    'run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" -o/dev/null'
register_conversion_case 'jpeg conversion with external palette' \
    'run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}"'
register_conversion_case 'indexed png scaling' \
    'run_img2sixel -7 -w300 "${snake_palette_png}"'
register_conversion_case 'terminal palette with repeated background' \
    'run_img2sixel -7 -w100 -h100 -bxterm16 -B"#aB3" -B"#aB3" \
        "${IMAGES_DIR}/snake.pbm"'
register_conversion_case 'inspect mode with stucki and background' \
    'run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}"'
register_conversion_case 'vt340 colour mode' \
    'run_img2sixel -bvt340color "${snake_ppm}"'
register_conversion_case 'vt340 monochrome mode' \
    'run_img2sixel -bvt340mono "${snake_tga}"'
register_conversion_case 'grayscale pack depth 1' \
    'run_img2sixel -bgray1 -w120 "${snake_tga}"'
register_conversion_case 'grayscale pack depth 2' \
    'run_img2sixel -bgray2 -w120 "${snake_tga}"'
register_conversion_case 'grayscale pack depth 4' \
    'run_img2sixel -bgray4 -w120 "${snake_tga}"'
register_conversion_case 'grayscale pack depth 8' \
    'run_img2sixel -bgray8 -w120 "${snake_tga}"'
register_conversion_case 'inspect mode with burkes dithering' \
    'run_img2sixel -I -8 -dburkes -B"#ffffffffffff" \
        "${snake_ascii_ppm}" >/dev/null'
register_conversion_case 'inspect mode with colour limit' \
    'run_img2sixel -I -C10 -djajuni "${snake_png}"'
register_conversion_case 'inspect mode with auto encoder' \
    'run_img2sixel -I -Eauto "${snake_ascii_pgm}"'
register_conversion_case 'inspect mode with atkinson dithering' \
    'run_img2sixel -I -datkinson "${snake_ascii_pbm}"'
register_conversion_case 'plain grayscale png conversion' \
    'run_img2sixel "${snake_gray_png}"'
register_conversion_case 'palette override on grayscale png' \
    'run_img2sixel -m "${map8_palette}" "${snake_gray_png}"'
register_conversion_case 'palette option tolerates rgb png' \
    'run_img2sixel -m "${snake_gray_png}" "${snake_png}"'
register_conversion_case 'crop with diffusion X' \
    'run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}"'
register_conversion_case 'crop with diffusion A and scaling' \
    'run_img2sixel -c200x200+100+100 -w400 -da_dither "${snake_gray_png}"'
register_conversion_case 'inspect default grayscale png' \
    'run_img2sixel -I "${snake_gray_png}"'
register_conversion_case 'inspect default grayscale jpg' \
    'run_img2sixel -I "${snake_gray_jpg}"'
register_conversion_case 'palette stacking accepts multiples' \
    'run_img2sixel -m "${map8_six}" -m "${map8_six}" "${snake_six}"'
register_conversion_case 'explicit width on sixel input' \
    'run_img2sixel -w200 -p8 "${snake_six}"'
register_conversion_case 'crop on sixel input' \
    'run_img2sixel -c200x200+2000+2000 "${snake_six}"'
register_conversion_case 'terminal palette target on sixel input' \
    'run_img2sixel -bxterm16 "${snake_six}"'
register_conversion_case 'encode flag on sixel input' \
    'run_img2sixel -e "${snake_six}"'
register_conversion_case 'inspect mode on sixel input' \
    'run_img2sixel -I "${snake_six}"'
register_conversion_case 'inspect diffusion with width on sixel input' \
    'run_img2sixel -I -da_dither -w100 "${snake_six}"'
register_conversion_case 'inspect diffusion with height on sixel input' \
    'run_img2sixel -I -dx_dither -h100 "${snake_six}"'
register_conversion_case 'inspect complex scaling and filters' \
    'run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}"'
register_conversion_case 'inspect verbose alternate filters' \
    'run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone \
        -rhamming -thls "${IMAGES_DIR}/snake.bmp"'
register_conversion_case 'palette override with automatic format detection' \
    'run_img2sixel -m "${IMAGES_DIR}/map8.png" -w200 -fauto -rwelsh \
        "${egret_jpg}"'
register_conversion_case 'palette override with bicubic resampling' \
    'run_img2sixel -m "${IMAGES_DIR}/map16.png" -w100 -hauto -rbicubic \
        -dauto "${snake_ppm}"'
register_conversion_case 'palette count and gamma tuning' \
    'run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}"'
register_conversion_case 'verbose palette tuning path' \
    'run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone "${snake_jpg}"'
register_conversion_case 'minimal palette configuration' \
    'run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}"'
register_conversion_case 'single colour palette with packed output' \
    'run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" && printf "\033[*1z"'
register_conversion_case 'encoder with preview flag' \
    'run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}"'
register_conversion_case 'inspect preview path exits cleanly' \
    'run_img2sixel -e -i -P "${snake_jpg}" >/dev/null'
register_conversion_case 'nested conversion stability' \
    'run_img2sixel -w204 -h204 "${snake_png}" | run_img2sixel >/dev/null'

case_total=${#CONVERSION_DESCRIPTIONS[@]}
tap_plan "${case_total}"

for index in "${!CONVERSION_DESCRIPTIONS[@]}"; do
    description=${CONVERSION_DESCRIPTIONS[${index}]}
    snippet=${CONVERSION_SNIPPETS[${index}]}
    tap_case "${description}" execute_conversion_case "${snippet}"
done
