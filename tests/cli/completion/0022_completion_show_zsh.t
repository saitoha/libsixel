#!/bin/sh
# TAP test verifying zsh completion output from img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/completion.zsh"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
              -1 zsh >"${output_file}" || {
    echo "not ok" 1 - "zsh completion output failed"
    exit 0
}

zsh_header_found=0
while IFS= read -r line; do
    case "${line}" in
        *"#compdef img2sixel"*)
            zsh_header_found=1
            break
            ;;
    esac
done < "${output_file}"

test "${zsh_header_found}" -eq 1 || {
    echo "not ok" 1 - "missing zsh completion header"
    exit 0
}

echo "ok" 1 - "zsh completion output available"
exit 0
