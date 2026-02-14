#!/bin/sh
# TAP test verifying zsh completion output from img2sixel.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/completion.zsh"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${top_srcdir}/converters/shell-completion"


printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_DIR

if run_img2sixel -1 zsh >"${output_file}"; then
    if grep '#compdef img2sixel' "${output_file}" >/dev/null 2>&1; then
        pass 1 "zsh completion output available"
    else
        fail 1 "missing zsh completion header"
    fi
else
    fail 1 "zsh completion output failed"
fi

exit "${status}"
