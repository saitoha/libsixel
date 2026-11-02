#!/bin/sh
# Enumerate physical case ownership, including invalid and symbolic entries.
set -eu
root=$1
checker=${0%/*}/../awk/test-layout.awk
walk() {
    test -d "$1" && test -r "$1" && test -x "$1" || return 1
    for entry in "$1"/*; do
        case "$entry" in "$root/_static"|"$root/_support"|"$root/_artifacts"|"$root/data") continue ;; esac
        if test -L "$entry"; then
            kind=S
            test ! -d "$entry" || kind=L
            printf '%s|%s\n' "$kind" "${entry#"$root"/}"
        elif test -d "$entry"; then
            walk "$entry" || return 1
        elif test -f "$entry"; then
            printf 'F|%s\n' "${entry#"$root"/}"
        fi
    done
}
# Publish no partial inventory to the checker after a traversal failure.
inventory=$(mktemp "${TMPDIR:-/tmp}/sixel-layout-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
walk "$root" > "$inventory" || exit 1
awk -F '|' -f "$checker" "$inventory"
