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

expected_dcs_crc="302131327e2d2131327e1b5c"

# Generate a checksum from the DCS payload emitted by img2sixel. This avoids
# invoking the test builtin with missing arguments when the pipeline yields no
# bytes (for example, because of platform-specific filtering on MSYS).
dcs_checksum() {
    scale_args=$1

    checksum=$(printf '\033Pq"1;1;1;1!6~\033\\' \
        | run_img2sixel -rne ${scale_args} 2>>"${log_file}" \
        | tr '#' '\n' | tail -n +3 \
        | od -An -tx1 | tr -d ' \n') || checksum=""

    printf '%s' "${checksum}"
}

check_dcs_crc() {
    case_no=$1
    scale_args=$2
    description=$3

    digest=$(dcs_checksum "${scale_args}")

    if [ -z "${digest}" ]; then
        fail "${case_no}" "${description} (no checksum produced)"
        return
    fi

    if [ "x${digest}" = "x${expected_dcs_crc}" ]; then
        pass "${case_no}" "${description}"
    else
        fail "${case_no}" "${description}"
    fi
}

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

check_dcs_crc 5 "-w200%" "width scaling preserves DCS coordinates"

check_dcs_crc 6 "-h200%" "height scaling preserves DCS coordinates"

check_dcs_crc 7 "-h200% -wauto" \
    "automatic width with height scaling stays consistent"

check_dcs_crc 8 "-hauto -w12" \
    "automatic height with width scaling stays consistent"

check_dcs_crc 9 "-h12 -w200%" \
    "combined absolute and percentage scaling consistent"

if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" \
    <"${snake_jpg}" 2>>"${log_file}"; then
    pass 10 "explicit dimensions and palette options succeeded"
else
    fail 10 "explicit dimensions and palette options failed"
fi

exit "${status}"
