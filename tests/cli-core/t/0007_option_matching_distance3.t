#!/bin/sh
# TAP test verifying distance-3 desampling typo is rejected with diagnostics.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/cli_core_common.sh"
cli_core_setup "img2sixel-option-matching"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

echo "1..1"

label="distance3"
err_file="${artifact_dir}/${label}.err"
out_file="${artifact_dir}/${label}.sixel"

rm -f "${err_file}" "${out_file}"

if run_img2sixel -r zzzzz "${images_dir}/snake.png" \
        >"${out_file}" 2>"${err_file}"; then
    cli_core_fail 1 "distance-3 typo unexpectedly succeeded"
    exit "${status}"
fi

if grep -F 'specified desampling method is not supported.' "${err_file}" \
        >/dev/null 2>&1; then
    cli_core_pass 1 "distance-3 typo reports diagnostic"
else
    cli_core_fail 1 "missing diagnostic for distance-3 typo"
    printf '--- stderr ---\n' >>"${log_file}"
    cat "${err_file}" >>"${log_file}" 2>/dev/null || :
fi

exit "${status}"
