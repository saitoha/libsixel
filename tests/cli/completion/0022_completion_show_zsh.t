#!/bin/sh
# TAP test verifying zsh completion output from img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/completion.zsh"

run_img2sixel --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
              -1 zsh >"${output_file}" || {
    echo "not ok" 1 - "zsh completion output failed"
    exit 0
}

grep '#compdef img2sixel' "${output_file}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "missing zsh completion header"
    exit 0
}

echo "ok" 1 - "zsh completion output available"
exit 0
