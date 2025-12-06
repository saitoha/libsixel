#!/bin/sh
# TAP test covering img2sixel mapfile import and export options.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/mapfile-io.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${tmp_dir}"

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

count_palette_entries() {
    printf '%s\n' "$1" \
        | grep -E '^[[:space:]]*[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+' \
        | wc -l
}

echo "1..3"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

map8_png="${images_dir}/map8.png"
snake_png="${images_dir}/snake.png"

require_file "${map8_png}"
require_file "${snake_png}"

pal_output="${tmp_dir}/quantized.pal"

aim="exporting palette to PAL file records requested size"
if run_img2sixel -p 8 -M "${pal_output}" "${map8_png}" -o /dev/null \
        2>>"${log_file}"; then
    if [ ! -s "${pal_output}" ]; then
        fail ${case_id} "${aim} (empty output)"
    else
        pal_header=$(head -n 1 "${pal_output}" || true)
        pal_count=$(sed -n '3p' "${pal_output}" || true)

        if printf '%s' "${pal_header}" | grep -q '^JASC-PAL$' \
                && [ "${pal_count}" = "8" ]; then
            pass ${case_id} "${aim}"
        else
            fail ${case_id} "${aim} (unexpected header or count)"
        fi
    fi
else
    fail ${case_id} "${aim} (conversion failure)"
fi
case_id=$((case_id + 1))

aim="stdout GPL palette matches requested color count"
if gpl_output=$(run_img2sixel -p 4 -M gpl:- "${snake_png}" -o /dev/null \
        2>>"${log_file}"); then
    color_rows=$(count_palette_entries "${gpl_output}")

    if [ "${color_rows}" -eq 4 ]; then
        pass ${case_id} "${aim}"
    else
        fail ${case_id} "${aim} (found ${color_rows} entries)"
    fi
else
    fail ${case_id} "${aim} (conversion failure)"
fi
case_id=$((case_id + 1))

inline_palette=$(cat <<'PAL'
GIMP Palette
Name: inline-map
Columns: 3
#
255 0 0
0 255 0
0 0 255
PAL
)
inline_palette_file="${tmp_dir}/inline.gpl"
printf '%s\n' "${inline_palette}" >"${inline_palette_file}"

aim="inline GPL map constrains exported palette"
if palette_out=$(run_img2sixel -m "gpl:${inline_palette_file}" -M gpl:- \
        "${snake_png}" -o /dev/null 2>>"${log_file}"); then
    color_rows=$(count_palette_entries "${palette_out}")

    if [ "${color_rows}" -eq 3 ] \
            && printf '%s\n' "${palette_out}" \
                | grep -Eq '^[[:space:]]*255[[:space:]]+0[[:space:]]+0([[:space:]]|$)' \
            && printf '%s\n' "${palette_out}" \
                | grep -Eq '^[[:space:]]*0[[:space:]]+255[[:space:]]+0([[:space:]]|$)' \
            && printf '%s\n' "${palette_out}" \
                | grep -Eq '^[[:space:]]*0[[:space:]]+0[[:space:]]+255([[:space:]]|$)'; then
        pass ${case_id} "${aim}"
    else
        fail ${case_id} "${aim} (palette entries unexpected)"
    fi
else
    fail ${case_id} "${aim} (conversion failure)"
fi

exit "${status}"
