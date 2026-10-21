#!/bin/sh
# Verify that documents, sections and paragraphs under docs/ stay within the
# length limits recorded in tools/docs/length-limits.tsv.
# Policy: docs/AGENTS.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
limits="tools/docs/length-limits.tsv"
checker="$src_root/tests/_static/awk/docs-length-limits.awk"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-docs-length-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

test -f "$src_root/$limits" || {
    echo "not ok 1 - documentation stays within length limits"
    echo "# missing $limits"
    exit 1
}

(cd "$src_root" && find docs -type f -name '*.md') | LC_ALL=C sort \
    > "$tmpdir/documents"

# Run from the source root so that reported paths are repository-relative.
# Document paths contain no white space; the EXTRA_DIST check relies on that too.
# shellcheck disable=SC2046
(cd "$src_root" && "${AWK:-awk}" -F '\t' -f "$checker" "$limits" \
    $(cat "$tmpdir/documents")) > "$tmpdir/errors" || true

test ! -s "$tmpdir/errors" || {
    echo "not ok 1 - documentation stays within length limits"
    sed 's/^/# /' "$tmpdir/errors"
    echo "# Split oversized material as described in docs/AGENTS.md;"
    echo "# raising a limit requires maintainer approval."
    exit 1
}

echo "ok 1 - documentation stays within length limits"
exit 0
