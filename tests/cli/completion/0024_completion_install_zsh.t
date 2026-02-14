#!/bin/sh
# TAP test verifying zsh completion installation from img2sixel.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if ! command -v zsh >/dev/null; then
    skip_all "zsh is not found"
fi

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${top_srcdir}/converters/shell-completion"
completion_home="${ARTIFACT_LOCAL_DIR}/home"
target_path="${completion_home}/.zfunc/_img2sixel"
rc_path="${completion_home}/.zshrc"


rm -rf "${completion_home}"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_HOME
export IMG2SIXEL_COMPLETION_DIR

if ! run_img2sixel -2 zsh > "${ARTIFACT_LOCAL_DIR}/output.txt"; then
    fail 1 "zsh completion install failed"
fi

if [ -f "${target_path}" ] && \
        grep '#compdef img2sixel' "${target_path}" >/dev/null 2>&1 && \
        grep "fpath+=(\"\$HOME/.zfunc\")" "${rc_path}" >/dev/null 2>&1 && \
        grep 'autoload -Uz compinit && compinit -u' "${rc_path}" >/dev/null 2>&1; then
    pass 1 "zsh completion installed"
else
    fail 1 "zsh completion not installed"
fi

exit "${status}"
