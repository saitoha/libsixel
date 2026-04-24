#!/bin/sh
# Emit TAP for private lookup backend symbol export checks.

set -eu

echo "1..1"

src_root=$1
build_root=${TOP_BUILDDIR:-$src_root}

if ! command -v nm >/dev/null 2>&1; then
    echo "ok 1 # SKIP nm not found"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-lookup-exports-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

symbols=$tmpdir/symbols.txt
forbidden=$tmpdir/forbidden.txt
inputs=$tmpdir/inputs.txt

: > "$inputs"
: > "$symbols"

for candidate in \
    "$build_root/src/.libs/libsixel.dylib" \
    "$build_root/src/.libs/libsixel.so" \
    "$build_root/src/.libs/libsixel.a" \
    "$build_root/src/libsixel.1.dylib" \
    "$build_root/src/libsixel.so" \
    "$build_root/src/libsixel.a" \
    "$build_root/src/libsixel.lib"
do
    test -f "$candidate" || continue
    printf '%s\n' "$candidate" >> "$inputs"
done

if test ! -s "$inputs"; then
    find "$build_root/src" -type f \
        \( -name '*lookup-8bit*.o' -o -name '*lookup-float32*.o' \) \
        >> "$inputs" 2>/dev/null || true
fi

if test ! -s "$inputs"; then
    echo "ok 1 # SKIP no built lookup artifacts under $build_root/src"
    exit 0
fi

while IFS= read -r artifact; do
    test -n "$artifact" || continue
    nm -gU "$artifact" >> "$symbols" 2>/dev/null && continue
    nm -g "$artifact" >> "$symbols" 2>/dev/null && continue
    nm "$artifact" >> "$symbols" 2>/dev/null || true
done < "$inputs"

awk '
{
    symbol = $NF
    sub(/^_/, "", symbol)
    if (symbol ~ /^sixel_lookup_(8bit|float32)_(configure|map_pixel)/ ||
            symbol == "sixel_certlut_free") {
        print symbol
    }
}
' "$symbols" | LC_ALL=C sort -u > "$forbidden"

if test -s "$forbidden"; then
    echo "not ok 1 - lookup backend legacy exports are private"
    sed 's/^/# /' "$forbidden"
    exit 1
fi

echo "ok 1 - lookup backend legacy exports are private"
