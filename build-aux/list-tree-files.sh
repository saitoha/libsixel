#!/bin/sh
# Enumerate support/build files by basename patterns, without findutils.
# Runtime case discovery deliberately uses the fixed-depth bulk glob instead.
set -eu
LC_ALL=C
export LC_ALL

test "$#" -ge 2 && test -d "$1" || {
    echo "Usage: $0 <directory> <basename-pattern>..." >&2
    exit 1
}

walk_files() {
    test -r "$1" && test -x "$1" || return 1
    # Positional parameters keep recursive calls from overwriting the parent
    # directory. Shell globs visit directories only, never symlink cycles.
    for pattern do
        test "$pattern" != "$1" || continue
        # Deliberate pathname expansion; source names contain no newlines.
        # shellcheck disable=SC2086
        for file in "$1"/$pattern; do
            test -f "$file" && test ! -L "$file" || continue
            printf '%s\n' "$file"
        done
    done
    for directory in "$1"/*/; do
        test -d "$directory" && test ! -L "${directory%/}" || continue
        case ${directory%/} in
            */_artifacts) continue ;;
        esac
        # Substitute the directory argument while preserving all patterns.
        descend_files "${directory%/}" "$@" || return $?
    done
}

descend_files() {
    directory=$1
    shift 2
    walk_files "$directory" "$@"
}

walk_files "$@"
