#!/bin/sh
# TAP test verifying combined bash/zsh completion output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
output_file="${ARTIFACT_LOCAL_DIR}/completion-all.txt"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

if run_img2sixel -1 all >"${output_file}"; then
    if grep -F '# bash completion for img2sixel' "${output_file}" >/dev/null 2>&1 && \
            grep -F '#compdef img2sixel' "${output_file}" >/dev/null 2>&1; then
        pass 1 "combined completion output available"
    else
        fail 1 "combined completion headers are missing"
    fi
else
    fail 1 "combined completion output failed"
fi

exit "${status}"
