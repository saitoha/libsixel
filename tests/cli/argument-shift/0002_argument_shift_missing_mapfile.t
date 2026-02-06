#!/bin/sh
# TAP test verifying -m requires an argument and does not shift unexpectedly.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_path="${top_srcdir}/tests/data/inputs/snake_64.jpg"
err_file="${ARTIFACT_LOCAL_DIR}/missing-map.err"
out_file="${ARTIFACT_LOCAL_DIR}/missing-map.out"

rm -f "${err_file}" "${out_file}"

if run_img2sixel -m -w 100 -h 100 "${image_path}" >"${out_file}" 2>"${err_file}"; then
    fail 1 "accepted -m without argument"
    exit 0
fi

if ! grep -q 'missing required argument for -m,--mapfile option' "${err_file}"; then
    fail 1 "no diagnostic for missing -m argument"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
fi

pass 1 "reports missing mapfile argument"
exit 0
