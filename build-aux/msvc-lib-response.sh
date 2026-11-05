#!/bin/sh
# Keep the MSYS2-to-native LIB boundary independent of object-list size.
# Arguments have already passed through the MSVC wrapper's path conversion.
set -eu
lib=$1
shift
work=$(mktemp -d "${TMPDIR:-/tmp}/sixel-msvc-lib-XXXXXX")
trap 'rm -rf "$work"' 0
trap 'exit 1' 1 2 3 15
# One record per argument lets awk quote the complete inventory in one pass.
# Newlines cannot be represented by LIB command files without changing tokens.
for arg do
    case $arg in
        *'
'*) echo 'msvc-lib-response: newline in an argument' >&2; exit 1 ;;
    esac
    printf '%s\n' "$arg"
done > "$work/input"
awk -v work="$work" '
function quoted(value,    out, i, ch, slashes, j) {
    # Ordinary object paths need no character-by-character escape pass.
    if (value !~ /[\\"]/) return "\"" value "\""
    out = "\""
    slashes = 0
    for (i = 1; i <= length(value); ++i) {
        ch = substr(value, i, 1)
        if (ch == "\\") { ++slashes; continue }
        if (ch == "\"") {
            for (j = 0; j < slashes * 2 + 1; ++j) out = out "\\"
        } else {
            for (j = 0; j < slashes; ++j) out = out "\\"
        }
        out = out ch
        slashes = 0
    }
    for (j = 0; j < slashes * 2; ++j) out = out "\\"
    return out "\""
}
/^@/ {
    # LIB allows multiple command files, but forbids nesting. Preserve the
    # position of existing files by ending the current direct-argument group.
    if (active) close(path)
    active = 0
    print "R" $0
    next
}
{
    if (!active) {
        path = work "/" ++segment ".rsp"
        print "N" path
        active = 1
    }
    print quoted($0) > path
}
' "$work/input" > "$work/invocation"
set --
while IFS= read -r record; do
    case $record in
        N*)
            native_path=$(cygpath -w "${record#N}")
            set -- "$@" "@$native_path"
            ;;
        R*) set -- "$@" "${record#R}" ;;
    esac
done < "$work/invocation"
# Keep the shell alive to clean command files on both success and failure.
# Native status is the script status; the EXIT trap must not mask it.
MSYS2_ARG_CONV_EXCL='*' "$lib" "$@"
