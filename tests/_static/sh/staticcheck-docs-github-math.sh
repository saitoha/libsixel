#!/bin/sh
# Verify that documentation math uses only GitHub.com-verified TeX commands.
# Policy: docs/AGENTS.md

set -eu

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
allowlist="$src_root/tests/_static/data/github-math-macros.txt"
checker="$src_root/tests/_static/awk/docs-github-math.awk"
errors=$(mktemp "${TMPDIR:-/tmp}/libsixel-docs-github-math-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -f "$errors"
}
trap cleanup EXIT HUP INT TERM

find "$src_root/docs" -type f -name '*.md' -exec \
    "${AWK:-awk}" -v allowlist_path="$allowlist" \
    -f "$checker" {} + > "$errors"

test ! -s "$errors" || {
    echo "not ok 1 - documentation math uses verified GitHub commands"
    sed 's/^/# /' "$errors"
    exit 1
}

echo "ok 1 - documentation math uses verified GitHub commands"
exit 0
