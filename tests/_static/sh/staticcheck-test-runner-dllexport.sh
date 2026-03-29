#!/bin/sh
# Emit TAP for test_runner calls that require dllexport in sixel.h.in.

set -eu

echo "1..1"

src_root=$1
header_file=$src_root/include/sixel.h.in
tests_root=$src_root/tests

if test ! -f "$header_file" || test ! -d "$tests_root"; then
    echo "ok 1 # SKIP missing include/sixel.h.in or tests directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-dllexport-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

decls_file=$tmpdir/decls.tsv
calls_file=$tmpdir/calls.tsv
missing_file=$tmpdir/missing.tsv

awk '
BEGIN {
    export_marker = ""
}
{
    if ($0 ~ /^[[:space:]]*(SIXELAPI|SIXEL_INTERNAL_API).*sixel_[A-Za-z0-9_]+[[:space:]]*\(/) {
        match($0, /sixel_[A-Za-z0-9_]+[[:space:]]*\(/)
        token = substr($0, RSTART, RLENGTH)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        printf "%s\t%d\t1\n", name, NR
        export_marker = ""
        next
    }

    if ($0 ~ /^[[:space:]]*SIXELAPI([[:space:]]|$)/) {
        export_marker = "SIXELAPI"
        next
    }
    if ($0 ~ /^[[:space:]]*SIXEL_INTERNAL_API([[:space:]]|$)/) {
        export_marker = "SIXEL_INTERNAL_API"
        next
    }

    if (match($0, /^[[:space:]]*sixel_[A-Za-z0-9_]+[[:space:]]*\(/)) {
        token = substr($0, RSTART, RLENGTH)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        exported = 0
        if (export_marker == "SIXELAPI" || export_marker == "SIXEL_INTERNAL_API") {
            exported = 1
        }
        printf "%s\t%d\t%d\n", name, NR, exported
        export_marker = ""
        next
    }

    if ($0 !~ /^[[:space:]]*$/) {
        export_marker = ""
    }
}
' "$header_file" > "$decls_file"

find "$tests_root" -type f -name '*.c' -exec awk '
{
    line = $0
    while (match(line, /sixel_[A-Za-z0-9_]+[[:space:]]*\(/)) {
        token = substr(line, RSTART, RLENGTH)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        printf "%s\t%s:%d\n", name, FILENAME, NR
        line = substr(line, RSTART + RLENGTH)
    }
}
' {} + | LC_ALL=C sort -u > "$calls_file"

if test ! -s "$calls_file"; then
    echo "ok 1 # SKIP no sixel_* calls found in tests"
    exit 0
fi

awk '
BEGIN {
    FS = "\t"
}
FNR == NR {
    called[$1] = 1
    next
}
{
    name = $1
    line_no = $2
    exported = $3
    if (called[name] && exported == 0) {
        printf "%s\t%s\n", name, line_no
    }
}
' "$calls_file" "$decls_file" > "$missing_file"

if test ! -s "$missing_file"; then
    echo "ok 1 - test_runner sixel.h.in calls are dllexport-ready"
    exit 0
fi

echo "not ok 1 - test_runner sixel.h.in calls are dllexport-ready"
awk '
BEGIN {
    FS = "\t"
}
FNR == NR {
    missing_line[$1] = $2
    next
}
{
    name = $1
    callsite = $2
    if (name in missing_line) {
        printf "# include/sixel.h.in:%s %s lacks SIXELAPI/SIXEL_INTERNAL_API (called from %s)\n",
               missing_line[name], name, callsite
    }
}
' "$missing_file" "$calls_file"
exit 1
