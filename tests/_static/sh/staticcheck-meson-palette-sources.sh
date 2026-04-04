#!/bin/sh
# Emit TAP for Meson palette source list consistency checks.

set -eu

echo "1..1"

src_root=$1
src_meson=$src_root/src/meson.build
amalg_meson=$src_root/amalgamation/meson.build
src_dir=$src_root/src

if test ! -f "$src_meson" || test ! -f "$amalg_meson" || test ! -d "$src_dir"; then
    echo "ok 1 # SKIP missing src/meson.build, amalgamation/meson.build, or src directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-meson-palette-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

src_entries=$tmpdir/src_entries.txt
unit_entries=$tmpdir/unit_entries.txt
header_entries=$tmpdir/header_entries.txt
palette_c=$tmpdir/palette_c.txt
palette_h=$tmpdir/palette_h.txt
missing=$tmpdir/missing.txt

awk '
/^srcs[[:space:]]*=[[:space:]]*\[/ { in_list = 1; next }
in_list && /^[[:space:]]*\]/ { in_list = 0; next }
!in_list { next }
{
    line = $0
    while (match(line, /\047[^\047]+\047/)) {
        token = substr(line, RSTART + 1, RLENGTH - 2)
        print token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$src_meson" | LC_ALL=C sort -u > "$src_entries"

awk '
/^amalgamation_units[[:space:]]*=[[:space:]]*files\(/ { in_list = 1; next }
in_list && /^[[:space:]]*\)/ { in_list = 0; next }
!in_list { next }
{
    line = $0
    while (match(line, /\047[^\047]+\047/)) {
        token = substr(line, RSTART + 1, RLENGTH - 2)
        print token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$amalg_meson" | LC_ALL=C sort -u > "$unit_entries"

awk '
/^amalgamation_headers[[:space:]]*=[[:space:]]*files\(/ { in_list = 1; next }
in_list && /^[[:space:]]*\)/ { in_list = 0; next }
!in_list { next }
{
    line = $0
    while (match(line, /\047[^\047]+\047/)) {
        token = substr(line, RSTART + 1, RLENGTH - 2)
        print token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$amalg_meson" | LC_ALL=C sort -u > "$header_entries"

find "$src_dir" -maxdepth 1 -type f -name 'palette-*.c' \
    -exec basename {} \; | LC_ALL=C sort -u > "$palette_c"
find "$src_dir" -maxdepth 1 -type f -name 'palette-*.h' \
    -exec basename {} \; | LC_ALL=C sort -u > "$palette_h"

status=0

while IFS= read -r name; do
    test -n "$name" || continue
    if ! grep -Fxq "$name" "$src_entries"; then
        echo "# src/meson.build: srcs is missing $name" >> "$missing"
        status=1
    fi
    if ! grep -Fxq "../src/$name" "$unit_entries"; then
        echo "# amalgamation/meson.build: amalgamation_units is missing ../src/$name" \
            >> "$missing"
        status=1
    fi
done < "$palette_c"

while IFS= read -r name; do
    test -n "$name" || continue
    if ! grep -Fxq "$name" "$src_entries"; then
        echo "# src/meson.build: srcs is missing $name" >> "$missing"
        status=1
    fi
    if ! grep -Fxq "../src/$name" "$header_entries"; then
        echo "# amalgamation/meson.build: amalgamation_headers is missing ../src/$name" \
            >> "$missing"
        status=1
    fi
done < "$palette_h"

if test "$status" -eq 0; then
    echo "ok 1 - meson palette sources are in sync"
    exit 0
fi

echo "not ok 1 - meson palette sources are in sync"
cat "$missing"
exit 1
