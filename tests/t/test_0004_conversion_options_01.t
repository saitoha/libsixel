#!/bin/sh
# TAP test exercising img2sixel conversion options and coordinate stability.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/conversion-options-01.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..10"

snake_jpg="${images_dir}/snake.jpg"
snake_png="${images_dir}/snake.png"
snake_six="${images_dir}/snake.six"

require_file "${snake_jpg}"
require_file "${snake_png}"
require_file "${snake_six}"

snake_roundtrip="${tmp_dir}/snake.sixel"
snake_scaling="${tmp_dir}/snake2.sixel"
snake_dims="${tmp_dir}/snake3.sixel"

if run_img2sixel "${snake_jpg}" -datkinson -flum -save \
    | run_img2sixel | tee "${snake_roundtrip}" >/dev/null 2>>"${log_file}"; then
    pass 1 "round-trip conversion with dithering and gamma succeeded"
else
    fail 1 "round-trip conversion failed"
fi

if run_img2sixel -sa "${snake_jpg}" >/dev/null 2>>"${log_file}"; then
    fail 2 "ambiguous select-color prefix accepted"
else
    pass 2 "ambiguous select-color prefix rejected"
fi

if run_img2sixel -shist "${snake_jpg}" >/dev/null 2>>"${log_file}"; then
    pass 3 "unique select-color prefix accepted"
else
    fail 3 "unique select-color prefix failed"
fi

if run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null 2>>"${log_file}"; then
    pass 4 "scaling with histogram and background succeeded"
else
    fail 4 "scaling with histogram and background failed"
fi

echo "\\033Pq\"1;1;1;1!6~\\033\\" \
    | run_img2sixel -rne -w200% 2>>"${log_file}" \
    | tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' \
    | xargs test 302131327e2d2131327e1b5c =
if [ $? -eq 0 ]; then
    pass 5 "width scaling preserves DCS coordinates"
else
    fail 5 "width scaling distorted coordinates"
fi

echo "\\033Pq\"1;1;1;1!6~\\033\\" \
    | run_img2sixel -rne -h200% 2>>"${log_file}" \
    | tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' \
    | xargs test 302131327e2d2131327e1b5c =
if [ $? -eq 0 ]; then
    pass 6 "height scaling preserves DCS coordinates"
else
    fail 6 "height scaling distorted coordinates"
fi

echo "\\033Pq\"1;1;1;1!6~\\033\\" \
    | run_img2sixel -rne -h200% -wauto 2>>"${log_file}" \
    | tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' \
    | xargs test 302131327e2d2131327e1b5c =
if [ $? -eq 0 ]; then
    pass 7 "automatic width with height scaling stays consistent"
else
    fail 7 "automatic width with height scaling changed coordinates"
fi

echo "\\033Pq\"1;1;1;1!6~\\033\\" \
    | run_img2sixel -rne -hauto -w12 2>>"${log_file}" \
    | tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' \
    | xargs test 302131327e2d2131327e1b5c =
if [ $? -eq 0 ]; then
    pass 8 "automatic height with width scaling stays consistent"
else
    fail 8 "automatic height with width scaling changed coordinates"
fi

echo "\\033Pq\"1;1;1;1!6~\\033\\" \
    | run_img2sixel -rne -h12 -w200% 2>>"${log_file}" \
    | tr '#' '\n' | tail -n +3 | od -An -tx1 | tr -d ' ' \
    | xargs test 302131327e2d2131327e1b5c =
if [ $? -eq 0 ]; then
    pass 9 "combined absolute and percentage scaling consistent"
else
    fail 9 "combined absolute and percentage scaling changed coordinates"
fi

if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" \
    <"${snake_jpg}" 2>>"${log_file}"; then
    pass 10 "explicit dimensions and palette options succeeded"
else
    fail 10 "explicit dimensions and palette options failed"
fi

exit "${status}"
