#!/usr/bin/env bash
# Exercise a wide range of img2sixel conversion options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test4] conversion options'

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
    run_img2sixel "${snake_jpg}" -datkinson -flum -saverage | \
        run_img2sixel | tee "${TMP_DIR}/snake.sixel" >/dev/null
    # Check percentage scaling, histogram stats, and background overrides.
    run_img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram < \
        "${snake_jpg}" | tee "${TMP_DIR}/snake2.sixel" >/dev/null
    # Ensure width scaling preserves expected DCS coordinates.
    printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -r nearest -w200% | \
        tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
        xargs test 302131327e2d2131327e1b5c =
    # Ensure height scaling preserves expected DCS coordinates.
    printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -r nearest -h200% | \
        tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
        xargs test 302131327e2d2131327e1b5c =
    # Ensure automatic width cooperates with explicit height scaling.
    printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -r nearest -h200% -wauto | \
        tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
        xargs test 302131327e2d2131327e1b5c =
    # Ensure automatic height cooperates with explicit width scaling.
    printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -r nearest -hauto -w12 | \
        tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
        xargs test 302131327e2d2131327e1b5c =
    # Ensure combined absolute and percentage scaling stays consistent.
    printf '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -r nearest -h12 -w200% | \
        tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' | \
        xargs test 302131327e2d2131327e1b5c =
    # Exercise explicit dimensions, dithering, and terminal palette output.
    run_img2sixel -w210 -h210 -djajuni -bxterm256 -o \
        "${TMP_DIR}/snake3.sixel" < "${snake_jpg}"
    # Confirm long option forms operate correctly.
    run_img2sixel --height=100 --diffusion=atkinson \
        --outfile="${TMP_DIR}/snake4.sixel" < "${snake_jpg}"
    # Convert GIF using precise scaling, background, and filter settings.
    run_img2sixel -w105% -h100 -B'#000000000' -rnearest < "${snake_gif}"
    # Check automatic scaling and Gaussian filter on TGA input.
    run_img2sixel -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto \
        "${snake_tga}"
    # Convert TIFF with palette sizing, centering, and custom colour math.
    run_img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning \
        -dstucki -thls "${snake_tiff}" -o/dev/null
    # Ensure palette auto selection with encoder sizing tweak.
    run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null
    # Verify explicit palette file with encoder sizing tweak.
    run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" -o/dev/null
    # Verify fast encoder path with external 16-colour palette.
    run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}"
    # Confirm large width scaling on indexed PNG input.
    run_img2sixel -7 -w300 "${snake_palette_png}"
    # Confirm repeated background overrides under terminal palette mode.
    run_img2sixel -7 -w100 -h100 -bxterm16 -B'#aB3' -B'#aB3' \
        "${IMAGES_DIR}/snake.pbm"
    # Inspect mode with Stucki dithering and custom background.
    run_img2sixel -I -dstucki -thls -B'#a0B030' "${snake_ppm}"
    # Validate VT340 colour mode rendering.
    run_img2sixel -bvt340color "${snake_ppm}"
    # Validate VT340 monochrome mode rendering.
    run_img2sixel -bvt340mono "${snake_tga}"
    # Confirm grayscale packing at 1-bit depth.
    run_img2sixel -bgray1 -w120 "${snake_tga}"
    # Confirm grayscale packing at 2-bit depth.
    run_img2sixel -bgray2 -w120 "${snake_tga}"
    # Confirm grayscale packing at 4-bit depth.
    run_img2sixel -bgray4 -w120 "${snake_tga}"
    # Confirm grayscale packing at 8-bit depth.
    run_img2sixel -bgray8 -w120 "${snake_tga}"
    # Ensure inspect mode with Atkinson dithering respects white background.
    run_img2sixel -I -8 -dburkes -B'#ffffffffffff' "${snake_ascii_ppm}" \
        > /dev/null
    # Verify inspect mode with colour count limit.
    run_img2sixel -I -C10 -djajuni "${snake_png}"
    # Verify inspect mode auto encoder on ASCII PGM input.
    run_img2sixel -I -Eauto "${snake_ascii_pgm}"
    # Verify inspect mode with Atkinson dithering on ASCII PBM input.
    run_img2sixel -I -datkinson "${snake_ascii_pbm}"
    # Confirm grayscale PNG conversion works without extras.
    run_img2sixel "${snake_gray_png}"
    # Confirm palette override accepts indexed PNG input.
    run_img2sixel -m "${map8_palette}" "${snake_gray_png}"
    # Ensure palette option tolerates non-palette PNG input.
    run_img2sixel -m "${snake_gray_png}" "${snake_png}"
    # Validate crop with diffusion X dither.
    run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}"
    # Validate crop with diffusion A dither and scaling.
    run_img2sixel -c200x200+100+100 -w400 -da_dither "${snake_gray_png}"
    # Inspect grayscale PNG in default inspect mode.
    run_img2sixel -I "${snake_gray_png}"
    # Inspect grayscale JPEG in default inspect mode.
    run_img2sixel -I "${snake_gray_jpg}"
    # Confirm palette stacking accepts multiple files.
    run_img2sixel -m "${map8_six}" -m "${map8_six}" "${snake_six}"
    # Confirm explicit width and palette sizing on SIXEL input.
    run_img2sixel -w200 -p8 "${snake_six}"
    # Confirm crop on SIXEL input.
    run_img2sixel -c200x200+2000+2000 "${snake_six}"
    # Confirm terminal palette target with SIXEL input.
    run_img2sixel -bxterm16 "${snake_six}"
    # Confirm encode flag on SIXEL input.
    run_img2sixel -e "${snake_six}"
    # Confirm inspect mode on SIXEL input.
    run_img2sixel -I "${snake_six}"
    # Confirm inspect with diffusion on SIXEL input respecting width.
    run_img2sixel -I -da_dither -w100 "${snake_six}"
    # Confirm inspect with diffusion on SIXEL input respecting height.
    run_img2sixel -I -dx_dither -h100 "${snake_six}"
    # Validate complex inspect run mixing scaling and filters.
    run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}"
    # Validate verbose inspect run with alternate filters.
    run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone \
        -rhamming -thls "${IMAGES_DIR}/snake.bmp"
    # Validate palette override with automatic format detection.
    run_img2sixel -m "${IMAGES_DIR}/map8.png" -w200 -fauto -rwelsh \
        "${egret_jpg}"
    # Validate palette override with bicubic resampling.
    run_img2sixel -m "${IMAGES_DIR}/map16.png" -w100 -hauto -rbicubic \
        -dauto "${snake_ppm}"
    # Exercise palette count and gamma tuning.
    run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}"
    # Exercise verbose palette tuning path.
    run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone "${snake_jpg}"
    # Exercise minimal palette configuration.
    run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}"
    # Exercise single-colour palette and packed output.
    run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" && printf '\033[*1z'
    # Confirm encoder and preview flags cooperate.
    run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}"
    # Confirm inspect preview path exits cleanly.
    run_img2sixel -e -i -P "${snake_jpg}" >/dev/null
    # Confirm nested conversion is stable.
    run_img2sixel -w204 -h204 "${snake_png}" | run_img2sixel >/dev/null
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'img2sixel conversion matrix'
else
    tap_not_ok 1 'img2sixel conversion matrix' \
        "See $(tap_log_hint) for details."
fi
