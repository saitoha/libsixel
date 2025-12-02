#!/bin/sh
# TAP test covering additional img2sixel conversion and inspection paths.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/conversion-options-04.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..17"

snake_six="${images_dir}/snake.six"
snake_ppm="${images_dir}/snake.ppm"
snake_png="${images_dir}/snake.png"
snake_bmp="${images_dir}/snake.bmp"
snake_jpg="${images_dir}/snake.jpg"
egret_jpg="${images_dir}/egret.jpg"
map8_png="${images_dir}/map8.png"
map16_png="${images_dir}/map16.png"

require_file "${snake_six}"
require_file "${snake_ppm}"
require_file "${snake_png}"
require_file "${snake_bmp}"
require_file "${snake_jpg}"
require_file "${egret_jpg}"
require_file "${map8_png}"
require_file "${map16_png}"

if run_img2sixel -c200x200+2000+2000 "${snake_six}" \
        >"${output_dir}/case01.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Sixel cropping tolerates large offsets"
else
    fail ${case_id} "Sixel cropping with large offsets fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -bxterm16 "${snake_six}" >"${output_dir}/case02.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "xterm16 preset re-encodes Sixel"
else
    fail ${case_id} "xterm16 preset failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -e "${snake_six}" >"${output_dir}/case03.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "direct Sixel encode emits data"
else
    fail ${case_id} "direct Sixel encode failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -I "${snake_six}" >"${output_dir}/case04.txt" \
        2>>"${log_file}"; then
    pass ${case_id} "Sixel metadata inspection succeeds"
else
    fail ${case_id} "Sixel metadata inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -da_dither -w100 "${snake_six}" \
        >"${output_dir}/case05.txt" 2>>"${log_file}"; then
    pass ${case_id} "alternate ordered dither inspection works"
else
    fail ${case_id} "alternate ordered dither inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -dx_dither -h100 "${snake_six}" \
        >"${output_dir}/case06.txt" 2>>"${log_file}"; then
    pass ${case_id} "X ordered dither inspection works"
else
    fail ${case_id} "X ordered dither inspection fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${output_dir}/case07.txt" \
        2>>"${log_file}"; then
    pass ${case_id} "PPM inspection tolerates aggressive scaling"
else
    fail ${case_id} "PPM inspection with scaling fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone \
        -rhamming -thls "${snake_bmp}" >"${output_dir}/case08.txt" \
        2>>"${log_file}"; then
    pass ${case_id} "BMP inspection with filters succeeds"
else
    fail ${case_id} "BMP inspection with filters fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${map8_png}" -w200 -fau -rwelsh "${egret_jpg}" \
        >"${output_dir}/case09.sixel" 2>>"${log_file}"; then
    pass ${case_id} "JPEG conversion using palette and Welsh filter"
else
    fail ${case_id} "JPEG palette Welsh conversion fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${map16_png}" -w100 -hauto -rbicubic -dauto \
        "${snake_ppm}" >"${output_dir}/case10.sixel" 2>>"${log_file}"; then
    pass ${case_id} "PPM conversion with 16-colour palette works"
else
    fail ${case_id} "PPM conversion with 16-colour palette fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 "${snake_jpg}" \
        >"${output_dir}/case11.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Lanczos2 scaling with palette limit succeeds"
else
    fail ${case_id} "Lanczos2 scaling with palette limit fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone \
        "${snake_jpg}" >"${output_dir}/case12.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Lanczos2 scaling without diffusion succeeds"
else
    fail ${case_id} "Lanczos2 scaling without diffusion fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        >"${output_dir}/case13.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Lanczos3 scaling with two-colour palette works"
else
    fail ${case_id} "Lanczos3 scaling with two-colour palette fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" \
        >"${output_dir}/case14.sixel" 2>>"${log_file}"; then
    printf '\033[*1z' >"${output_dir}/case14-trailer.txt"
    pass ${case_id} "monochrome frame with trailer succeeds"
else
    fail ${case_id} "monochrome frame with trailer fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -e -h140 -rlanczos4 -P "${snake_jpg}" \
        >"${output_dir}/case15.sixel" 2>>"${log_file}"; then
    pass ${case_id} "Lanczos4 scaling emits palette dump"
else
    fail ${case_id} "Lanczos4 scaling palette dump fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -e -i -P "${snake_jpg}" >"${output_dir}/case16.sixel" \
        2>>"${log_file}"; then
    pass ${case_id} "interlaced encode emits palette dump"
else
    fail ${case_id} "interlaced encode palette dump fails"
fi
case_id=$((case_id + 1))

if run_img2sixel -w204 -h204 "${snake_png}" \
        >"${output_dir}/case17-stage1.sixel" 2>>"${log_file}" && \
        run_img2sixel <"${output_dir}/case17-stage1.sixel" \
        >"${output_dir}/case17-stage2.sixel" 2>>"${log_file}"; then
    pass ${case_id} "two-pass Sixel conversion succeeds"
else
    fail ${case_id} "two-pass Sixel conversion fails"
fi

exit "${status}"
