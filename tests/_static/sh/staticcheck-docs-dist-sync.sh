#!/bin/sh
# Verify that every file under docs/ is distributed through EXTRA_DIST.
# Policy: docs/AGENTS.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-docs-dist-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

(cd "$src_root" && find docs -type f ! -name .DS_Store) | LC_ALL=C sort \
    > "$tmpdir/present"
# Every EXTRA_DIST entry is separated from its continuation backslash by a
# blank, so splitting on white space isolates the paths.
tr -s '[:space:]' '\n' < "$src_root/Makefile.am" | grep '^docs/' |
    LC_ALL=C sort -u > "$tmpdir/listed"

LC_ALL=C comm -23 "$tmpdir/present" "$tmpdir/listed" |
    sed 's/^/missing from Makefile.am EXTRA_DIST: /' > "$tmpdir/errors"
LC_ALL=C comm -13 "$tmpdir/present" "$tmpdir/listed" |
    sed 's/^/listed in Makefile.am but absent: /' >> "$tmpdir/errors"

test ! -s "$tmpdir/errors" || {
    echo "not ok 1 - every docs/ file is distributed"
    sed 's/^/# /' "$tmpdir/errors"
    exit 1
}

echo "ok 1 - every docs/ file is distributed"
exit 0
