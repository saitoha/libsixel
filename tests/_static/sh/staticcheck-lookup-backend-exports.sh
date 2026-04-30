#!/bin/sh
# Emit TAP for private lookup backend source visibility checks.

set -eu

echo "1..1"

src_root=$1

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-lookup-exports-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

forbidden=$tmpdir/forbidden.txt

if test ! -d "$src_root/src"; then
    echo "not ok 1 - lookup backend legacy exports are private"
    echo "# missing source directory: $src_root/src"
    exit 1
fi

find "$src_root/src" -type f -name '*.[ch]' -exec awk '
FNR == 1 {
    previous = ""
}
/^sixel_lookup_(8bit|float32)_(configure|map_pixel)[[:space:]]*\(/ ||
        /^sixel_certlut_free[[:space:]]*\(/ {
    if (previous !~ /^static([[:space:]]|$)/) {
        print FILENAME ":" FNR ":" $0
    }
}
{
    previous = $0
}
' {} + | LC_ALL=C sort -u > "$forbidden"

if test -s "$forbidden"; then
    echo "not ok 1 - lookup backend legacy exports are private"
    sed 's/^/# /' "$forbidden"
    exit 1
fi

echo "ok 1 - lookup backend legacy exports are private"
