#!/bin/sh
# TAP test verifying zsh completion installation from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

command -v zsh >/dev/null || {
    printf "1..0 # SKIP zsh is not found";
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

completion_dir="${TOP_SRCDIR}/converters/shell-completion"
completion_home="${ARTIFACT_LOCAL_DIR}/home"
target_path="${completion_home}/.zfunc/_img2sixel"
rc_path="${completion_home}/.zshrc"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_HOME
export IMG2SIXEL_COMPLETION_DIR

run_img2sixel -2 zsh >/dev/null || {
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
