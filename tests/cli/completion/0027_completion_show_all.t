#!/bin/sh
# TAP test verifying combined bash/zsh completion output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo '1..1'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/completion-all.txt"


${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -1 all >"${output_file}" || {
    echo "not ok" 1 - "combined completion output failed"
    exit 0
}

bash_header_found=0
zsh_header_found=0
while IFS= read -r line; do
    case "${line}" in
        *"# bash completion for img2sixel"*)
            bash_header_found=1
            ;;
        *"#compdef img2sixel"*)
            zsh_header_found=1
            ;;
    esac
done < "${output_file}"

test "${bash_header_found}" -eq 1 || {
    echo "not ok" 1 - "missing bash completion header in combined output"
    exit 0
}

test "${zsh_header_found}" -eq 1 || {
    echo "not ok" 1 - "missing zsh completion header in combined output"
    exit 0
}

echo "ok" 1 - "combined completion output available"
exit 0
