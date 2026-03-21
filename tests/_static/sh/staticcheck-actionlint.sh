#!/bin/sh
# Emit TAP for actionlint static check.

set -eu

src_root=$1
actionlint_bin=$2

if test -z "$actionlint_bin"; then
    echo "1..0 # SKIP actionlint not found"
    exit 0
fi

if test ! -x "$actionlint_bin" && ! command -v "$actionlint_bin" >/dev/null 2>&1; then
    echo "1..0 # SKIP actionlint executable not found: $actionlint_bin"
    exit 0
fi

tmpfile=`mktemp "${TMPDIR:-/tmp}/libsixel-actionlint-files-XXXXXX"`
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

cd "$src_root"

find .github/workflows -type f \( -name '*.yml' -o -name '*.yaml' \) \
    | LC_ALL=C sort > "$tmpfile" || true

total=`wc -l < "$tmpfile" | awk '{print $1}'`

echo "1..1"

if test "$total" -eq 0; then
    echo "# actionlint: no workflow files under .github/workflows"
    echo "ok 1 - actionlint"
    exit 0
fi

failed=0
index=0
while IFS= read -r workflow; do
    test -n "$workflow" || continue
    index=$((index + 1))
    echo "# [actionlint ${index}/${total}] ${workflow#./}"
    if ! "$actionlint_bin" "$workflow"; then
        failed=1
    fi
done < "$tmpfile"

if test "$failed" -eq 0; then
    echo "ok 1 - actionlint"
else
    echo "not ok 1 - actionlint"
    exit 1
fi
