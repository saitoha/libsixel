#!/bin/sh
# TAP test verifying ambiguous option prefix is rejected with a diagnostic.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "img2sixel-option-matching"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

label="prefix_ambiguous"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

rm -f "${err_file}" "${out_file}"

if run_img2sixel -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        >"${out_file}" 2>"${err_file}"; then
    cli_core_fail 1 "ambiguous prefix unexpectedly succeeded"
    exit "${status}"
fi

if grep -F 'ambiguous prefix "sie"' "${err_file}" >/dev/null 2>&1; then
    cli_core_pass 1 "ambiguous prefix reports diagnostic"
else
    cli_core_fail 1 "missing diagnostic for ambiguous prefix"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit "${status}"
