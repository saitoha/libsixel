#!/bin/sh
# Emit TAP ensuring deprecated diagnostic pragmas use configured guards.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - deprecated diagnostic pragmas use configured guards"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-deprecated-guard-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt

for scan_dir in "$src_root/src" "$src_root/include" "$src_root/tests"
do
    test -d "$scan_dir" || continue
    find "$scan_dir" -type f \( -name '*.c' -o -name '*.h' \) -print
done | LC_ALL=C sort | while IFS= read -r path
do
    awk '
    function starts_if(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef)([[:space:]]|$)/
    }
    function starts_elif(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*elif([[:space:]]|$)/
    }
    function starts_else(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*else([[:space:]]|$)/
    }
    function starts_endif(line) {
        return line ~ /^[[:space:]]*#[[:space:]]*endif([[:space:]]|$)/
    }
    function has_deprecated_guard(line) {
        return line !~ /^[[:space:]]*#[[:space:]]*ifndef([[:space:]]|$)/ &&
            line ~ /HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS/
    }
    starts_if($0) {
        depth++
        guarded[depth] = guarded[depth - 1] || has_deprecated_guard($0)
        next
    }
    starts_elif($0) {
        guarded[depth] = guarded[depth - 1] || has_deprecated_guard($0)
        next
    }
    starts_else($0) {
        guarded[depth] = guarded[depth - 1]
        next
    }
    starts_endif($0) {
        delete guarded[depth]
        if (depth > 0) {
            depth--
        }
        next
    }
    /^[[:space:]]*#[[:space:]]*pragma[[:space:]]+(GCC|clang)[[:space:]]+diagnostic[[:space:]]+ignored[[:space:]]+"-Wdeprecated-declarations"/ {
        if (!guarded[depth]) {
            printf "%s:%d:unguarded -Wdeprecated-declarations pragma\n",
                FILENAME, NR
        }
    }
    ' "$path"
done > "$bad"

if test -s "$bad"; then
    echo "not ok 1 - deprecated diagnostic pragmas use configured guards"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - deprecated diagnostic pragmas use configured guards"
exit 0
