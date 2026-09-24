#!/bin/sh
# Verify that the document-type inventory lists every Markdown document once.
# Policy: docs/AGENTS.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
inventory="$src_root/tools/docs/doc-types.tsv"
checker="$src_root/tests/_static/awk/doc-types-inventory.awk"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-doc-types-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

(cd "$src_root" && find docs -type f -name '*.md') | LC_ALL=C sort \
    > "$tmpdir/documents"

test -f "$inventory" || {
    echo "not ok 1 - document-type inventory matches docs/"
    echo "# missing tools/docs/doc-types.tsv"
    exit 1
}

"${AWK:-awk}" -F '\t' -v documents="$tmpdir/documents" \
    -f "$checker" "$inventory" > "$tmpdir/errors" || {
    echo "not ok 1 - document-type inventory matches docs/"
    LC_ALL=C sort "$tmpdir/errors" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - document-type inventory matches docs/"
exit 0
