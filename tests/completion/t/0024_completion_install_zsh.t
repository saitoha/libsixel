#!/bin/sh
# TAP test verifying zsh completion installation from img2sixel.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/completion.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

if ! command -v zsh >/dev/null; then
    skip_all "zsh is not found"
fi

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

completion_dir="${top_srcdir}/converters/shell-completion"
completion_dir=$(printf '%s' "${completion_dir}" | tr '\\\\' '/')
completion_source="${completion_dir}/zsh/_img2sixel"
completion_home="${artifact_dir}/home"
target_path="${completion_home}/.zfunc/_img2sixel"
rc_path="${completion_home}/.zshrc"

require_file "${completion_source}"
rm -rf "${completion_home}"
mkdir -p "${completion_home}"

printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_HOME="${completion_home}"
IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_HOME
export IMG2SIXEL_COMPLETION_DIR

if run_img2sixel -2 zsh >"${log_file}" 2>&1; then
    if [ -f "${target_path}" ] && \
            grep -F '#compdef img2sixel' \
            "${target_path}" >/dev/null 2>&1 && \
            grep -F 'fpath+=("$HOME/.zfunc")' \
            "${rc_path}" >/dev/null 2>&1 && \
            grep -F 'autoload -Uz compinit && compinit -u' \
            "${rc_path}" >/dev/null 2>&1; then
        pass 1 "zsh completion installed"
    else
        fail 1 "zsh completion not installed"
    fi
else
    fail 1 "zsh completion install failed"
fi

exit "${status}"
