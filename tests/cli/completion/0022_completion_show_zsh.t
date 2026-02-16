#!/bin/sh
# TAP test verifying zsh completion output from img2sixel.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/completion.zsh"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${TOP_SRCDIR}/converters/shell-completion"


printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_DIR

run_img2sixel -1 zsh >"${output_file}" || {
    fail 1 "zsh completion output failed"
    exit 0
}

grep '#compdef img2sixel' "${output_file}" >/dev/null 2>&1 || {
    fail 1 "missing zsh completion header"
    exit 0
}

pass 1 "zsh completion output available"

exit 0
