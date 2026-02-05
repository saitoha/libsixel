#!/bin/sh
# TAP test verifying distance-1 multi-match diffusion option is rejected.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "img2sixel-option-matching"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

echo "1..1"
set -v

label="distance1_multi"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

rm -f "${err_file}" "${out_file}"

if run_img2sixel -r hamning "${images_dir}/snake.png" \
        >"${out_file}" 2>"${err_file}"; then
    cli_core_fail 1 "distance-1 multi-match unexpectedly succeeded"
    exit "${status}"
fi

if grep -F 'specified desampling method is not supported.' "${err_file}" \
        >/dev/null 2>&1; then
    cli_core_pass 1 "distance-1 multi-match reports diagnostic"
else
    cli_core_fail 1 "missing diagnostic for distance-1 multi-match"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit "${status}"
