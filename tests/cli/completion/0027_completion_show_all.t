#!/bin/sh
# TAP test verifying combined bash/zsh completion output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo '1..1'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/completion-all.txt"


run_img2sixel -1 all >"${output_file}" || {
    echo "not ok" 1 - "combined completion output failed"
    exit 0
}

grep '# bash completion for img2sixel' "${output_file}" >/dev/null || {
    echo "not ok" 1 - "missing bash completion header in combined output"
    exit 0
}

grep '#compdef img2sixel' "${output_file}" >/dev/null || {
    echo "not ok" 1 - "missing zsh completion header in combined output"
    exit 0
}

echo "ok" 1 - "combined completion output available"
exit 0
