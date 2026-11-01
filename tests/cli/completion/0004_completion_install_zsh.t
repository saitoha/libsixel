#!/bin/sh
# TAP test verifying zsh completion installation from img2sixel.

set -eux

command -v zsh >/dev/null || {
    printf "1..0 # SKIP zsh is not found\n";
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

completion_dir="${TOP_SRCDIR}/converters/shell-completion"
completion_home="${ARTIFACT_LOCAL_DIR}/home"
target_path="${completion_home}/.zfunc/_img2sixel"
rc_path="${completion_home}/.zshrc"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              --env IMG2SIXEL_COMPLETION_DIR="${completion_dir}" \
              -2 zsh >/dev/null || {
    echo "not ok" 1 - "zsh completion install failed"
    exit 0
}

test -f "${target_path}" || {
    echo "not ok" 1 - "zsh completion not installed"
    exit 0
}

target_header_found=0
while IFS= read -r line; do
    case "${line}" in
        *"#compdef img2sixel"*)
            target_header_found=1
            break
            ;;
    esac
done < "${target_path}"

test "${target_header_found}" -eq 1 || {
    echo "not ok" 1 - "zsh completion not installed"
    exit 0
}

test -f "${rc_path}" || {
    echo "not ok" 1 - "zsh completion not installed"
    exit 0
}

fpath_line_found=0
compinit_line_found=0
while IFS= read -r line; do
    case "${line}" in
        "fpath+=(\"\$HOME/.zfunc\")")
            fpath_line_found=1
            ;;
        "autoload -Uz compinit && compinit -u")
            compinit_line_found=1
            ;;
    esac
done < "${rc_path}"

test "${fpath_line_found}" -eq 1 || {
    echo "not ok" 1 - "zsh completion not installed"
    exit 0
}

test "${compinit_line_found}" -eq 1 || {
    echo "not ok" 1 - "zsh completion not installed"
    exit 0
}

echo "ok" 1 - "zsh completion installed"
exit 0
