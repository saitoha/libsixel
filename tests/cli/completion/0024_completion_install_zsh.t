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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

completion_dir="${TOP_SRCDIR}/converters/shell-completion"
completion_home="${ARTIFACT_LOCAL_DIR}/home"
target_path="${completion_home}/.zfunc/_img2sixel"
rc_path="${completion_home}/.zshrc"

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              --env IMG2SIXEL_COMPLETION_DIR="${completion_dir}" \
              -2 zsh >/dev/null || {
    fail 1 "zsh completion install failed"
    exit 0
}

test -f "${target_path}" || {
    fail 1 "zsh completion not installed"
    exit 0
}

grep '#compdef img2sixel' "${target_path}" >/dev/null 2>&1 || {
    fail 1 "zsh completion not installed"
    exit 0
}

grep "fpath+=(\"\$HOME/.zfunc\")" "${rc_path}" >/dev/null 2>&1 || {
    fail 1 "zsh completion not installed"
    exit 0
}

grep 'autoload -Uz compinit && compinit -u' "${rc_path}" >/dev/null 2>&1 || {
    fail 1 "zsh completion not installed"
    exit 0
}

pass 1 "zsh completion installed"
exit 0
