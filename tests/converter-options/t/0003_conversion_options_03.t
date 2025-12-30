#!/bin/sh
# TAP test covering img2sixel palette, dithering, and grayscale outputs.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/conversion-options-03.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..24"

snake_jpg="${images_dir}/snake.jpg"
snake_png="${images_dir}/snake.png"
snake_gif="${images_dir}/snake.gif"
snake_tga="${images_dir}/snake.tga"
snake_tiff="${images_dir}/snake.tiff"
snake_pgm="${images_dir}/snake.pgm"
snake_ppm="${images_dir}/snake.ppm"
snake_ascii_ppm="${images_dir}/snake-ascii.ppm"
snake_ascii_pgm="${images_dir}/snake-ascii.pgm"
snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
snake_gray_png="${images_dir}/snake-grayscale.png"
snake_gray_jpg="${images_dir}/snake-grayscale.jpg"
snake_six="${images_dir}/snake.six"
map8_palette="${images_dir}/map8-palette.png"
map16_palette="${images_dir}/map16-palette.png"
map8_six="${images_dir}/map8.six"
snake_palette_png="${images_dir}/snake-palette.png"

require_file "${snake_jpg}"
require_file "${snake_png}"
require_file "${snake_six}"
require_file "${map8_palette}"
require_file "${map16_palette}"
require_file "${map8_six}"
require_file "${snake_palette_png}"

if run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" \
        -o/dev/null 2>>"${log_file}"; then
    pass ${case_id} "fast encoder with palette succeeds"
else
    fail ${case_id} "fast encoder with palette fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}" \
        >"${output_dir}/case02.sixel" 2>>"${log_file}"; then
    pass ${case_id} "16-colour palette conversion succeeds"
else
    fail ${case_id} "16-colour palette conversion fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -7 -w300 "${snake_palette_png}" \
        >"${output_dir}/case03.sixel" 2>>"${log_file}"; then
    pass ${case_id} "indexed PNG scales to large width"
else
    fail ${case_id} "indexed PNG scaling fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -7 -w100 -h100 -bxterm16 -B'#aB3' -B'#aB3' \
        "${images_dir}/snake.pbm" >"${output_dir}/case04.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "xterm palette overrides repeat"
else
    fail ${case_id} "xterm palette overrides fail"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -dstucki -thls -B'#a0B030' "${snake_ppm}" \
        >"${output_dir}/case05.txt" 2>>"${log_file}"; then
    pass ${case_id} "inspection with diffusion and background works"
else
    fail ${case_id} "inspection with diffusion failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -bvt340color "${snake_ppm}" \
        >"${output_dir}/case06.sixel" 2>>"${log_file}"; then
    pass ${case_id} "VT340 colour control sequences emitted"
else
    fail ${case_id} "VT340 colour control emission failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -bvt340mono "${snake_tga}" \
        >"${output_dir}/case07.sixel" 2>>"${log_file}"; then
    pass ${case_id} "VT340 monochrome control sequences emitted"
else
    fail ${case_id} "VT340 monochrome control failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -bgray1 -w120 "${snake_tga}" \
        >"${output_dir}/case08.sixel" 2>>"${log_file}"; then
    pass ${case_id} "1-bit grayscale output succeeds"
else
    fail ${case_id} "1-bit grayscale output fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -bgray2 -w120 "${snake_tga}" \
        >"${output_dir}/case09.sixel" 2>>"${log_file}"; then
    pass ${case_id} "2-bit grayscale output succeeds"
else
    fail ${case_id} "2-bit grayscale output fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -bgray4 -w120 "${snake_tga}" \
        >"${output_dir}/case10.sixel" 2>>"${log_file}"; then
    pass ${case_id} "4-bit grayscale output succeeds"
else
    fail ${case_id} "4-bit grayscale output fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -bgray8 -w120 "${snake_tga}" \
        >"${output_dir}/case11.sixel" 2>>"${log_file}"; then
    pass ${case_id} "8-bit grayscale output succeeds"
else
    fail ${case_id} "8-bit grayscale output fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -8 -dburkes -B'#ffffffffffff' "${snake_ascii_ppm}" \
        >"${output_dir}/case12.txt" 2>>"${log_file}"; then
    pass ${case_id} "ASCII PPM inspection honours diffusion"
else
    fail ${case_id} "ASCII PPM inspection failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -C10 -djajuni "${snake_png}" \
        >"${output_dir}/case13.txt" 2>>"${log_file}"; then
    pass ${case_id} "PNG inspection sets colour space"
else
    fail ${case_id} "PNG inspection colour space failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -Eauto "${snake_ascii_pgm}" \
        >"${output_dir}/case14.txt" 2>>"${log_file}"; then
    pass ${case_id} "ASCII PGM auto encoder inspection works"
else
    fail ${case_id} "ASCII PGM auto encoder inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -datkinson "${snake_ascii_pbm}" \
        >"${output_dir}/case15.txt" 2>>"${log_file}"; then
    pass ${case_id} "ASCII PBM Atkinson inspection works"
else
    fail ${case_id} "ASCII PBM Atkinson inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel "${snake_gray_png}" >"${output_dir}/case16.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "grayscale PNG conversion succeeds"
else
    fail ${case_id} "grayscale PNG conversion fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${map8_palette}" "${snake_gray_png}" \
        >"${output_dir}/case17.sixel" 2>>"${log_file}"; then
    pass ${case_id} "grayscale PNG with external palette works"
else
    fail ${case_id} "grayscale PNG palette conversion fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${snake_gray_png}" "${snake_png}" \
        >"${output_dir}/case18.sixel" 2>>"${log_file}"; then
    pass ${case_id} "grayscale palette applied to colour PNG"
else
    fail ${case_id} "grayscale palette application fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}" \
        >"${output_dir}/case19.sixel" 2>>"${log_file}"; then
    pass ${case_id} "cropping with X ordered dither succeeds"
else
    fail ${case_id} "cropping with X ordered dither fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -c200x200+100+100 -w400 -da_dither \
        "${snake_gray_png}" >"${output_dir}/case20.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "cropping with alternate dither succeeds"
else
    fail ${case_id} "cropping with alternate dither fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I "${snake_gray_png}" >"${output_dir}/case21.txt" \
        2>>"${log_file}"; then
    pass ${case_id} "grayscale PNG inspection succeeds"
else
    fail ${case_id} "grayscale PNG inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I "${snake_gray_jpg}" >"${output_dir}/case22.txt" \
        2>>"${log_file}"; then
    pass ${case_id} "grayscale JPEG inspection succeeds"
else
    fail ${case_id} "grayscale JPEG inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${map8_six}" -m "${map8_six}" "${snake_six}" \
        >"${output_dir}/case23.sixel" 2>>"${log_file}"; then
    pass ${case_id} "stacked palette files handled"
else
    fail ${case_id} "stacked palette files fail"
fi
case_id=$((case_id + 1))

if run_img2sixel -w200 -p8 "${snake_six}" \
        >"${output_dir}/case24.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Sixel resizing with palette limit succeeds"
else
    fail ${case_id} "Sixel resizing with palette limit fails"
fi

exit "${status}"
