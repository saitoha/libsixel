#!/bin/sh
# TAP test exercising mapfile input/output palette handling.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/mapfile.log"
output_dir="${artifact_dir}/outputs"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0
case_id=1

ensure_converter_available "ENABLE_CONVERTERS" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

snake_png="${images_dir}/snake.png"
require_file "${snake_png}"

echo "1..9"

act_palette="${tmp_dir}/palette.act"
if run_img2sixel -M "${act_palette}" -o "${tmp_dir}/act.six" \
        "${snake_png}" 2>>"${log_file}"; then
    act_size=$(wc -c <"${act_palette}")
    if [ "${act_size}" -eq 772 ]; then
        pass ${case_id} "ACT palette exported with correct length"
    else
        fail ${case_id} "ACT palette length mismatch (${act_size})"
    fi
else
    fail ${case_id} "ACT palette export failed"
fi
case_id=$((case_id + 1))

pal_default="${tmp_dir}/palette-default.pal"
if run_img2sixel -M "${pal_default}" -o "${tmp_dir}/pal-default.six" \
        "${snake_png}" 2>>"${log_file}"; then
    if head -n 1 "${pal_default}" | grep -q "JASC-PAL"; then
        pass ${case_id} "PAL export defaults to JASC layout"
    else
        fail ${case_id} "PAL export missing JASC header"
    fi
else
    fail ${case_id} "PAL default export failed"
fi
case_id=$((case_id + 1))

pal_stdout="${output_dir}/palette-stdout.pal"
if run_img2sixel -M pal:- -o "${tmp_dir}/pal-stdout.six" \
        "${snake_png}" >"${pal_stdout}" 2>>"${log_file}"; then
    if head -n 1 "${pal_stdout}" | grep -q "JASC-PAL"; then
        pass ${case_id} "PAL export supports type-prefixed stdout"
    else
        fail ${case_id} "PAL stdout header missing"
    fi
else
    fail ${case_id} "PAL stdout export failed"
fi
case_id=$((case_id + 1))

riff_palette="${tmp_dir}/palette-riff.pal"
if run_img2sixel -M pal-riff:"${riff_palette}" \
        -o "${tmp_dir}/pal-riff.six" "${snake_png}" 2>>"${log_file}"; then
    riff_header=$(dd if="${riff_palette}" bs=1 count=4 2>/dev/null |
        LC_ALL=C od -An -tx1 | tr -d ' \n')
    if [ "${riff_header}" = "52494646" ]; then
        pass ${case_id} "RIFF palette export honoured type prefix"
    else
        fail ${case_id} "RIFF palette header incorrect (${riff_header})"
    fi
else
    fail ${case_id} "RIFF palette export failed"
fi
case_id=$((case_id + 1))

gpl_palette="${tmp_dir}/palette-gpl.dat"
if run_img2sixel -M gpl:"${gpl_palette}" -o "${tmp_dir}/pal-gpl.six" \
        "${snake_png}" 2>>"${log_file}"; then
    if head -n 1 "${gpl_palette}" | grep -q "GIMP Palette"; then
        pass ${case_id} "GPL palette export handles custom names"
    else
        fail ${case_id} "GPL palette header missing"
    fi
else
    fail ${case_id} "GPL palette export failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -m gpl:"${gpl_palette}" \
        -o "${output_dir}/from-gpl.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-gpl.six" ]; then
        pass ${case_id} "GPL palette input via type prefix works"
    else
        fail ${case_id} "GPL palette conversion produced no data"
    fi
else
    fail ${case_id} "GPL palette conversion failed"
fi
case_id=$((case_id + 1))

if run_img2sixel -m "${act_palette}" -o "${output_dir}/from-act.six" \
        "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-act.six" ]; then
        pass ${case_id} "ACT palette input detected by extension"
    else
        fail ${case_id} "ACT palette conversion produced no data"
    fi
else
    fail ${case_id} "ACT palette conversion failed"
fi
case_id=$((case_id + 1))

riff_alias="${tmp_dir}/palette-riff-noext"
cp "${riff_palette}" "${riff_alias}"
if run_img2sixel -m pal-riff:"${riff_alias}" \
        -o "${output_dir}/from-riff.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-riff.six" ]; then
        pass ${case_id} "RIFF palette parsed with explicit type"
    else
        fail ${case_id} "RIFF palette conversion produced no data"
    fi
else
    fail ${case_id} "RIFF palette conversion failed"
fi
case_id=$((case_id + 1))

if cat "${gpl_palette}" | run_img2sixel -m gpl:- \
        -o "${output_dir}/from-stdin.six" "${snake_png}" 2>>"${log_file}"; then
    if [ -s "${output_dir}/from-stdin.six" ]; then
        pass ${case_id} "Palette input accepted from stdin"
    else
        fail ${case_id} "stdin palette conversion produced no data"
    fi
else
    fail ${case_id} "stdin palette conversion failed"
fi

exit "${status}"
