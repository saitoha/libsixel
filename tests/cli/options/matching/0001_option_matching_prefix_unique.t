#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "img2sixel-option-matching"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

label="prefix_unique"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
filtered_err="${ARTIFACT_LOCAL_DIR}/${label}.filtered.err"

cleanup_files() {
    rm -f "$@" || :
}

cleanup_files "${err_file}" "${out_file}" "${filtered_err}"

echo "1..1"
set -v

if run_img2sixel -y ser "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >"${out_file}" 2>"${err_file}"; then
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
            printf '%s\n' '--- stderr ---' >&2
            cat "${err_file}" >&2 2>/dev/null || :
            cleanup_files "${filtered_err}"
            exit "${status}"
        fi
    fi
    cleanup_files "${filtered_err}"
fi

cli_core_pass 1 "unique prefix is accepted"
exit "${status}"
