#!/bin/sh
# Emit TAP for test_runner calls that require dllexport in visible headers.

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
headers_file=$tmpdir/headers.txt

printf '%s\n' "$header_file" > "$headers_file"

find "$tests_root" -type f -name '*.c' -exec awk '
{
    if (match($0, /^[[:space:]]*#include[[:space:]]+"src\/[A-Za-z0-9_.\/-]+\.h"/)) {
        token = substr($0, RSTART, RLENGTH)
        sub(/^[[:space:]]*#include[[:space:]]+"/, "", token)
        sub(/"$/, "", token)
        print token
    }
}
' {} + | LC_ALL=C sort -u | awk -v root="$src_root" '
{
    printf "%s/%s\n", root, $0
}
' >> "$headers_file"
LC_ALL=C sort -u "$headers_file" -o "$headers_file"

# shellcheck disable=SC2016
tr '\n' '\0' < "$headers_file" | xargs -0 awk '
BEGIN {
    export_marker = ""
}
{
    if ($0 ~ /^[[:space:]]*(SIXELAPI|SIXEL_INTERNAL_API).*([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*\(/) {
        token = $0
        sub(/^.*[[:space:]]/, "", token)
        sub(/[[:space:]]*\(.*/, "", token)
        if (token ~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
            printf "%s\t%s:%d\t1\n", token, FILENAME, FNR
            export_marker = ""
            next
        }
    }

    if ($0 ~ /^[[:space:]]*SIXELAPI([[:space:]]|$)/) {
        export_marker = "SIXELAPI"
        next
    }
    if ($0 ~ /^[[:space:]]*SIXEL_INTERNAL_API([[:space:]]|$)/) {
        export_marker = "SIXEL_INTERNAL_API"
        next
    }

    if (match($0, /^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        token = substr($0, RSTART, RLENGTH)
        sub(/^[[:space:]]*/, "", token)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        exported = 0
        if (export_marker == "SIXELAPI" || export_marker == "SIXEL_INTERNAL_API") {
            exported = 1
        }
        printf "%s\t%s:%d\t%d\n", name, FILENAME, FNR, exported
        export_marker = ""
        next
    }

    if ($0 !~ /^[[:space:]]*$/) {
        export_marker = ""
    }
}
' | LC_ALL=C sort -u > "$decls_file"

find "$tests_root" -type f -name '*.c' -exec awk '
{
    line = $0
    while (match(line, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        token = substr(line, RSTART, RLENGTH)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        if (name != "if" &&
            name != "for" &&
            name != "while" &&
            name != "switch" &&
            name != "return" &&
            name != "sizeof") {
            printf "%s\t%s:%d\n", name, FILENAME, NR
        }
        line = substr(line, RSTART + RLENGTH)
    }
}
' {} + | LC_ALL=C sort -u > "$calls_file"

if test ! -s "$calls_file"; then
    echo "ok 1 # SKIP no callable symbol references found in tests"
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
    echo "ok 1 - test_runner header calls are dllexport-ready"
    exit 0
fi

echo "not ok 1 - test_runner header calls are dllexport-ready"
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
        printf "# %s %s lacks SIXELAPI/SIXEL_INTERNAL_API (called from %s)\n",
               missing_line[name], name, callsite
    }
}
' "$missing_file" "$calls_file"
exit 1
