#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/cli_core_common.sh"
cli_core_setup "img2sixel-option-matching"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

label="prefix_unique"
err_file="${artifact_dir}/${label}.err"
out_file="${artifact_dir}/${label}.sixel"
filtered_err="${artifact_dir}/${label}.filtered.err"

cleanup_files() {
    rm -f "$@" || :
}

cleanup_files "${err_file}" "${out_file}" "${filtered_err}"

echo "1..1"

if run_img2sixel -y ser "${images_dir}/snake.png" >"${out_file}" 2>"${err_file}"; then
    :
else
    cli_core_fail 1 "unique prefix was rejected"
    exit "${status}"
fi

if [ -s "${err_file}" ]; then
    if sed '1d' "${err_file}" \
            | grep -v '^+' \
            | grep -v 'img2sixel' \
            | grep -Ei 'error|warning|failed' \
            >"${filtered_err}"; then
        if [ -s "${filtered_err}" ]; then
            cli_core_fail 1 "unique prefix emitted diagnostics"
            printf '--- stderr ---\n' >>"${log_file}"
            cat "${err_file}" >>"${log_file}" 2>/dev/null || :
            cleanup_files "${filtered_err}"
            exit "${status}"
        fi
    fi
    cleanup_files "${filtered_err}"
fi

cli_core_pass 1 "unique prefix is accepted"
exit "${status}"
