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


command -v cmp >/dev/null 2>&1 || {
    printf "1..0 # SKIP cmp not available\n";
    exit 0
}

printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

LC_ALL=C sed -n "/ --[0-9a-zA-Z_@=~%?]/{
    /' /d
    s/.* \\(-.\\) .*/\\1/p
}" "${TOP_SRCDIR}/converters/shell-completion/bash/img2sixel" >"${bash_opts}" || {
    echo "not ok" 1 - "failed to parse bash completion"
    exit 0
}

LC_ALL=C sort "${bash_opts}" >"${bash_sorted}" || {
    echo "not ok" 1 - "failed to sort bash completion"
    exit 0
}

LC_ALL=C sed -n '/{-/s/.*{-\([^,}]*\).*/-\1/p' \
    "${TOP_SRCDIR}/converters/shell-completion/zsh/_img2sixel" >"${zsh_opts}" || {
    echo "not ok" 1 - "failed to parse zsh completion"
    exit 0
}

LC_ALL=C sort "${zsh_opts}" >"${zsh_sorted}" || {
    echo "not ok" 1 - "failed to sort zsh completion"
    exit 0
}

cmp -s "${bash_sorted}" "${zsh_sorted}" || {
    echo "not ok" 1 - "bash completion diverges from zsh completion"
    exit 0
}

echo "ok" 1 - "bash completion matches zsh completion"
exit 0
