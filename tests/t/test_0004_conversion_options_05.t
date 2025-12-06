#!/bin/sh
# TAP test covering mapfile input/output options for img2sixel.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/conversion-options-05.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

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

echo "1..6"

snake_png="${images_dir}/snake.png"
require_file "${snake_png}"

palette_gpl="${output_dir}/palette.gpl"
palette_pal="${output_dir}/palette.pal"
palette_unknown="${output_dir}/palette.bin"
palette_empty="${output_dir}/palette-empty.gpl"

if run_img2sixel -p16 -M "${palette_gpl}" "${snake_png}" \
        >"${output_dir}/case01.sixel" 2>>"${log_file}"; then
    if [ -s "${palette_gpl}" ] && \
            grep -q '^GIMP Palette' "${palette_gpl}"; then
        pass ${case_id} "GPL palette exported alongside sixel output"
    else
        fail ${case_id} "GPL palette missing or malformed"
    fi
else
    fail ${case_id} "img2sixel failed to export GPL palette"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${palette_gpl}" -w120 "${snake_png}" \
        >"${output_dir}/case02.sixel" 2>>"${log_file}"; then
    pass ${case_id} "GPL palette reused via -m"
else
    fail ${case_id} "GPL palette reuse failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -p32 -M "${palette_pal}" "${snake_png}" \
        >"${output_dir}/case03.sixel" 2>>"${log_file}"; then
    if [ -s "${palette_pal}" ] && \
            grep -q '^JASC-PAL' "${palette_pal}"; then
        pass ${case_id} "PAL (JASC) palette exported with format hint"
    else
        fail ${case_id} "PAL palette missing or malformed"
    fi
else
    fail ${case_id} "img2sixel failed to export PAL palette"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${palette_pal}" -7 "${snake_png}" \
        >"${output_dir}/case04.sixel" 2>>"${log_file}"; then
    pass ${case_id} "PAL palette reused via -m"
else
    fail ${case_id} "PAL palette reuse failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -M "${palette_unknown}" "${snake_png}" \
        >"${output_dir}/case05.sixel" 2>"${output_dir}/case05.err"; then
    fail ${case_id} "accepted mapfile-output without known format"
else
    if grep -qi 'unknown palette file extension' \
            "${output_dir}/case05.err"; then
        pass ${case_id} "rejected unknown palette extension"
    else
        fail ${case_id} "missing diagnostic for unknown palette extension"
    fi
fi
case_id=$((case_id + 1))

: >"${palette_empty}"
if run_img2sixel -m "${palette_empty}" "${snake_png}" \
        >"${output_dir}/case06.sixel" 2>"${output_dir}/case06.err"; then
    fail ${case_id} "accepted empty palette file"
else
    if grep -qi 'mapfile .* is empty' "${output_dir}/case06.err"; then
        pass ${case_id} "empty palette file rejected with diagnostic"
    else
        fail ${case_id} "missing diagnostic for empty palette file"
    fi
fi

exit "${status}"
