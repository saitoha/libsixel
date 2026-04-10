#!/bin/sh
# Emit TAP for temporal source/header one-to-one pairing in src/.

set -eu

echo "1..1"

src_root=$1
src_dir=$src_root/src

if test ! -d "$src_dir"; then
    echo "ok 1 # SKIP missing src directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-temporal-c-h-pair-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

c_bases=$tmpdir/c_bases.txt
h_bases=$tmpdir/h_bases.txt
missing=$tmpdir/missing.txt
status=0

find "$src_dir" -maxdepth 1 -type f -name 'dither-temporal-*.c' \
    -exec basename {} .c \; | LC_ALL=C sort -u > "$c_bases"
find "$src_dir" -maxdepth 1 -type f -name 'dither-temporal-*.h' \
    -exec basename {} .h \; | LC_ALL=C sort -u > "$h_bases"

while IFS= read -r base; do
    test -n "$base" || continue
    if ! grep -Fxq "$base" "$h_bases"; then
        echo "# src/$base.c: missing matching header src/$base.h" >> "$missing"
        status=1
    fi
done < "$c_bases"

while IFS= read -r base; do
    test -n "$base" || continue
    if ! grep -Fxq "$base" "$c_bases"; then
        echo "# src/$base.h: missing matching source src/$base.c" >> "$missing"
        status=1
    fi
done < "$h_bases"

if test "$status" -eq 0; then
    echo "ok 1 - temporal c/h files keep one-to-one pairing"
    exit 0
fi

echo "not ok 1 - temporal c/h files keep one-to-one pairing"
cat "$missing"
exit 1
