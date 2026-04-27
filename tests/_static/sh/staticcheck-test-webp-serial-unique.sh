#!/bin/sh
# Emit TAP for validating unique numeric prefixes in builtin WebP TAP files.

set -eu

src_root=$1
builtin_dir=$src_root/tests/loader/builtin

echo "1..1"

test -d "$builtin_dir" || {
    echo "not ok 1 - builtin webp TAP serial uniqueness"
    echo "# builtin test directory not found: $builtin_dir"
    exit 1
}

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-webp-test-serials-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

find "$builtin_dir" -maxdepth 1 -type f \
    -name '[0-9][0-9][0-9][0-9]_loader_builtin_webp_*.t' \
    -print | LC_ALL=C sort > "$tmpfile"

test -s "$tmpfile" || {
    echo "ok 1 # SKIP no builtin webp TAP tests found"
    exit 0
}

if ! awk '
{
    file = $0
    sub(/^.*\//, "", file)
    serial = substr(file, 1, 4)
    if (!(serial in seen)) {
        seen[serial] = file
        next
    }
    printf "# duplicate builtin webp TAP serial %s: %s <> %s\n",
        serial, seen[serial], file
    bad = 1
}
END {
    exit bad ? 1 : 0
}
' "$tmpfile"; then
    echo "not ok 1 - builtin webp TAP serial numbers are unique"
    exit 1
fi

echo "ok 1 - builtin webp TAP serial numbers are unique"
