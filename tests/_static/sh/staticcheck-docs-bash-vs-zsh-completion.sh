#!/bin/sh
# Emit TAP for Bash versus Zsh completion option parity.
# Policy: docs/cli/design-policy.md

set -eu

echo "1..1"

src_root=$1
bash_file="$src_root/converters/shell-completion/bash/img2sixel"
zsh_file="$src_root/converters/shell-completion/zsh/_img2sixel"

test -f "$bash_file" || {
    echo "ok 1 # SKIP missing Bash completion script"
    exit 0
}

test -f "$zsh_file" || {
    echo "ok 1 # SKIP missing Zsh completion script"
    exit 0
}

bash_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-bash-options-XXXXXX")
zsh_norm=$(mktemp "${TMPDIR:-/tmp}/libsixel-staticcheck-zsh-options-XXXXXX")

cleanup() {
    rm -f "$bash_norm" "$zsh_norm"
}
trap cleanup EXIT HUP INT TERM

awk '
$1 ~ /^-.$/ && $2 ~ /^--[a-z0-9][a-z0-9-]*$/ && $3 == "\\" {
    print $1, $2
}
' "$bash_file" | LC_ALL=C sort -u > "$bash_norm"

awk '
/^[[:space:]]*\{-[^,],--[a-z0-9][a-z0-9-]*=?\}/ {
    line = $0
    sub(/^[[:space:]]*\{/, "", line)
    sub(/\}.*/, "", line)
    count = split(line, field, ",")
    if (count == 2) {
        sub(/=$/, "", field[2])
        print field[1], field[2]
    }
}
' "$zsh_file" | LC_ALL=C sort -u > "$zsh_norm"

bash_count=$(wc -l < "$bash_norm")
zsh_count=$(wc -l < "$zsh_norm")
test "$bash_count" -ge 50 || {
    echo "not ok 1 - Bash completion option parser returned too few entries"
    echo "# bash=$bash_count zsh=$zsh_count"
    exit 1
}
test "$zsh_count" -ge 50 || {
    echo "not ok 1 - Zsh completion option parser returned too few entries"
    echo "# bash=$bash_count zsh=$zsh_count"
    exit 1
}

bash_sum=$(cksum < "$bash_norm")
zsh_sum=$(cksum < "$zsh_norm")

test "$bash_sum" = "$zsh_sum" || {
    echo "not ok 1 - Bash completion diverges from Zsh completion"
    echo "# Bash parse:"
    sed 's/^/#   /' "$bash_norm"
    echo "# Zsh parse:"
    sed 's/^/#   /' "$zsh_norm"
    exit 1
}

echo "ok 1 - Bash and Zsh completion option sets match"
