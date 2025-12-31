#!/bin/sh
# TAP test verifying -m requires an argument and does not shift unexpectedly.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/cli_core_common.sh"
cli_core_setup "img2sixel-argument-shift"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

abs_top=""
if abs_top=$(cd "${top_srcdir}" && pwd); then
    :
else
    echo "failed to resolve source root" >&2
    exit 1
fi

image_path="${abs_top}/images/snake.jpg"
require_file "${image_path}"

echo "1..1"

err_file="${artifact_dir}/missing-map.err"
out_file="${artifact_dir}/missing-map.out"

rm -f "${err_file}" "${out_file}"

if run_img2sixel -m -w 100 -h 100 "${image_path}" \
        >"${out_file}" 2>"${err_file}"; then
    cli_core_fail 1 "accepted -m without argument"
else
    if grep -q 'missing required argument for -m,--mapfile option' \
            "${err_file}"; then
        cli_core_pass 1 "reports missing mapfile argument"
    else
        cli_core_fail 1 "no diagnostic for missing -m argument"
        printf '--- stderr ---\n' >>"${log_file}"
        cat "${err_file}" >>"${log_file}" 2>/dev/null || :
    fi
fi

exit "${status}"
