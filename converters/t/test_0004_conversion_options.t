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
# Verify explicit palette file with encoder sizing tweak.
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
