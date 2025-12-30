#!/bin/sh
# TAP test covering img2sixel output selection and diverse input formats.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/conversion-options-02.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..8"

snake_jpg="${images_dir}/snake.jpg"
snake_png="${images_dir}/snake.png"
snake_gif="${images_dir}/snake.gif"
snake_tga="${images_dir}/snake.tga"
snake_tiff="${images_dir}/snake.tiff"
snake_pgm="${images_dir}/snake.pgm"

require_file "${snake_jpg}"
require_file "${snake_png}"
require_file "${snake_gif}"
require_file "${snake_tga}"
require_file "${snake_tiff}"
require_file "${snake_pgm}"

prefixed_png="${tmp_dir}/snake-prefix.png"
target_png="${tmp_dir}/snake.png"
filename_png="${tmp_dir}/snake-filename.png"
longopt_sixel="${tmp_dir}/snake4.sixel"

if run_img2sixel -o "png:${prefixed_png}" "${snake_jpg}" 2>>"${log_file}"; then
    if [ -s "${prefixed_png}" ]; then
        pass 1 "prefixed PNG output created"
    else
        fail 1 "prefixed PNG output missing"
    fi
else
    fail 1 "prefixed PNG conversion failed"
fi

if run_img2sixel -o "png:${target_png}" "${snake_jpg}" 2>>"${log_file}"; then
    if [ -s "${target_png}" ]; then
        pass 2 "prefixed PNG writes to explicit path"
    else
        fail 2 "prefixed PNG did not produce file"
    fi
else
    fail 2 "prefixed PNG conversion failed"
fi

if run_img2sixel -o "${filename_png}" "${snake_jpg}" 2>>"${log_file}"; then
    header=$(od -An -tx1 -N8 "${filename_png}" | tr -d ' \n')
    if [ "${header}" = "89504e470d0a1a0a" ]; then
        pass 3 "filename-driven PNG output uses PNG header"
    else
        fail 3 "filename-driven PNG header incorrect"
    fi
else
    fail 3 "filename-driven PNG conversion failed"
fi

if run_img2sixel --height=100 --diffusion=atkinson \
    --outfile="${longopt_sixel}" <"${snake_jpg}" 2>>"${log_file}"; then
    pass 4 "long option forms accepted"
else
    fail 4 "long option forms failed"
fi

if run_img2sixel -w105% -h100 -B"#000000000" -rne <"${snake_gif}" \
    >"${tmp_dir}/snake-gif.sixel" 2>>"${log_file}"; then
    pass 5 "GIF conversion with filters succeeded"
else
    fail 5 "GIF conversion with filters failed"
fi

if run_img2sixel -7 -sauto -w100 -rga -qauto -dburkes -tauto \
    "${snake_tga}" >"${tmp_dir}/snake-tga.sixel" 2>>"${log_file}"; then
    pass 6 "TGA conversion with scaling succeeded"
else
    fail 6 "TGA conversion with scaling failed"
fi

if run_img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki \
    -thls "${snake_tiff}" -o/dev/null 2>>"${log_file}"; then
    pass 7 "TIFF conversion with palette controls succeeded"
else
    fail 7 "TIFF conversion with palette controls failed"
fi

if run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null \
    2>>"${log_file}"; then
    pass 8 "PGM encode flag cooperates with palette auto-selection"
else
    fail 8 "PGM encode flag failed"
fi

exit "${status}"
