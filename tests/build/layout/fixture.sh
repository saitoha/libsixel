#!/bin/sh
# Source-only layout controls create the physical files being classified.
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-layout-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
add() { mkdir -p "$work/${1%/*}"; : > "$work/$1"; }
cases() {
    mkdir -p "$work/${2:-owner/behavior}"
    awk -v directory="$work/${2:-owner/behavior}" -v count="$1" 'BEGIN {
        for (i = 1; i <= count; i++) {
            path = sprintf("%s/%04d_observation.t", directory, i)
            printf "" > path
            close(path)
        }
    }'
}

accept() { sh "$src_root/tests/_static/sh/check-test-layout.sh" "$work"; }
reject() {
    result=0
    errors=$(sh "$src_root/tests/_static/sh/check-test-layout.sh" "$work") || result=$?
    test "$result" -ne 0 || return 1
    case "$errors" in *"$1"*) return 0 ;; *) printf '%s\n' "$errors" >&2; return 1 ;; esac
}
