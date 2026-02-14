#!/bin/sh
# TAP test verifying combined bash/zsh completion output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

output_file="${ARTIFACT_LOCAL_DIR}/completion-all.txt"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

run_img2sixel -1 all >"${output_file}" || {
    fail 1 "combined completion output failed"
    exit 0
}

grep '# bash completion for img2sixel' "${output_file}" >/dev/null || {
    fail 1 "missing bash completion header in combined output"
    exit 0
}

grep '#compdef img2sixel' "${output_file}" >/dev/null || {
    fail 1 "missing zsh completion header in combined output"
    exit 0
}

pass 1 "combined completion output available"
exit 0
