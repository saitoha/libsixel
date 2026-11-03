#!/bin/sh
# Every source fixture under a test data directory must reach source releases.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
inventory=$(mktemp "${TMPDIR:-/tmp}/libsixel-fixture-dist-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --exclude-name .DS_Store > "$inventory" || {
    echo 'not ok 1 - cannot enumerate test fixtures'
    exit 0
}
awk -v root="$src_root/tests" -v makefile="$src_root/tests/Makefile.am" \
    -f "$src_root/tests/_static/awk/test-fixture-distribution.awk" \
    "$inventory" >&2 || {
    echo 'not ok 1 - test fixture directories are distributed'
    exit 0
}
echo 'ok 1 - test fixture directories are distributed'
exit 0
