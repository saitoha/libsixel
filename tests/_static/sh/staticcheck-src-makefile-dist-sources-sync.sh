#!/bin/sh
# Emit TAP for src/Makefile.am and src/Makefile.in source-list sync.

set -eu

src_root=$1
am_file=$src_root/src/Makefile.am
in_file=$src_root/src/Makefile.in
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-src-makefile-sync-XXXXXX")
am_list=$tmpdir/am.list
in_list=$tmpdir/in.list
missing=$tmpdir/missing.list

cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

echo "1..1"

if test ! -f "$am_file"; then
    echo "not ok 1 - src Makefile source lists stay synchronized"
    echo "# missing file: $am_file"
    exit 1
fi

if test ! -f "$in_file"; then
    echo "not ok 1 - src Makefile source lists stay synchronized"
    echo "# missing file: $in_file"
    exit 1
fi

awk '
{
    line = $0
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.-]+/)) {
        print substr(line, RSTART, RLENGTH)
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$am_file" | LC_ALL=C sort -u > "$am_list"

awk '
{
    line = $0
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.-]+/)) {
        print substr(line, RSTART, RLENGTH)
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$in_file" | LC_ALL=C sort -u > "$in_list"

comm -23 "$am_list" "$in_list" > "$missing"

if test -s "$missing"; then
    echo "not ok 1 - src Makefile source lists stay synchronized"
    sed 's/^/# missing in src\/Makefile.in: /' "$missing"
    exit 1
fi

echo "ok 1 - src Makefile source lists stay synchronized"
