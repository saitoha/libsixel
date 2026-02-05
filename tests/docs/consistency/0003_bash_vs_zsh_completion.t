#!/bin/sh
# TAP test comparing img2sixel bash and zsh completion definitions.

set -eux

bash_opts="${ARTIFACT_LOCAL_DIR}/options-bash.txt"
zsh_opts="${ARTIFACT_LOCAL_DIR}/options-zsh.txt"
bash_sorted="${ARTIFACT_LOCAL_DIR}/options-bash-sorted.txt"
zsh_sorted="${ARTIFACT_LOCAL_DIR}/options-zsh-sorted.txt"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



die_skip() {
    reason=$1
    echo "1..1"
    echo "ok 1 - skip ${reason}"
    exit 0
}

require_file "${top_srcdir}/converters/shell-completion/bash/img2sixel"
require_file "${top_srcdir}/converters/shell-completion/zsh/_img2sixel"

if ! command -v diff >/dev/null 2>&1; then
    die_skip "diff not available"
fi

printf '1..1\n'
set -v

if grep -E ' --[0-9a-zA-Z_@=~%?]' \
        "${top_srcdir}/converters/shell-completion/bash/img2sixel" \
        | grep -v "' " \
        | sed 's/.* \(-.\) .*/\1/' >"${bash_opts}"; then
    :
else
    fail 1 "failed to parse bash completion"
fi

if LC_ALL=C sort "${bash_opts}" >"${bash_sorted}"; then
    :
else
    fail 1 "failed to sort bash completion"
fi

if grep -F '{-' "${top_srcdir}/converters/shell-completion/zsh/_img2sixel" \
        | cut -f1 -d, \
        | cut -f2 -d'{' >"${zsh_opts}"; then
    :
else
    fail 1 "failed to parse zsh completion"
fi

if LC_ALL=C sort "${zsh_opts}" >"${zsh_sorted}"; then
    :
else
    fail 1 "failed to sort zsh completion"
fi

if diff -u "${bash_sorted}" "${zsh_sorted}"; then
    pass 1 "bash completion matches zsh completion"
else
    fail 1 "bash completion diverges from zsh completion"
fi

exit "${status}"
