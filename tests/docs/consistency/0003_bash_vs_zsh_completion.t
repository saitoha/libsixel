#!/bin/sh
# TAP test comparing img2sixel bash and zsh completion definitions.

set -eux

bash_opts="${ARTIFACT_LOCAL_DIR}/options-bash.txt"
zsh_opts="${ARTIFACT_LOCAL_DIR}/options-zsh.txt"
bash_sorted="${ARTIFACT_LOCAL_DIR}/options-bash-sorted.txt"
zsh_sorted="${ARTIFACT_LOCAL_DIR}/options-zsh-sorted.txt"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

command -v diff >/dev/null 2>&1 || {
    printf "1..0 # SKIP diff not available\n";
    exit 0
}

printf '1..1\n'
set -v

grep ' --[0-9a-zA-Z_@=~%?]' \
    "${TOP_SRCDIR}/converters/shell-completion/bash/img2sixel" \
    | grep -v "' " \
    | sed 's/.* \(-.\) .*/\1/' >"${bash_opts}" || {
    fail 1 "failed to parse bash completion"
    exit 0
}

LC_ALL=C sort "${bash_opts}" >"${bash_sorted}" || {
    fail 1 "failed to sort bash completion"
    exit 0
}

grep '{-' "${TOP_SRCDIR}/converters/shell-completion/zsh/_img2sixel" \
    | cut -f1 -d, \
    | cut -f2 -d'{' >"${zsh_opts}" || {
    fail 1 "failed to parse zsh completion"
    exit 0
}

LC_ALL=C sort "${zsh_opts}" >"${zsh_sorted}" || {
    fail 1 "failed to sort zsh completion"
    exit 0
}

diff -u "${bash_sorted}" "${zsh_sorted}" || {
    fail 1 "bash completion diverges from zsh completion"
    exit 0
}

pass 1 "bash completion matches zsh completion"
exit 0
