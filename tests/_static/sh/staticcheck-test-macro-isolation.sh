#!/bin/sh
# Differing function-like test macros must not leak across source units.
# Policy: docs/build/autotools.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
# Keep the complete inventory in one awk process despite host argv limits.
inventory=$(mktemp "${TMPDIR:-/tmp}/libsixel-test-macros-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.c' --exclude-path '*/data/*' \
    --exclude-name 'test-runner-amalgamation.c' > "$inventory" || {
    echo 'not ok 1 - cannot enumerate test macro owners'
    exit 0
}
awk -f "$src_root/tests/_static/awk/test-macro-isolation.awk" \
    "$inventory" >&2 || {
    echo 'not ok 1 - differing test macros remain local to their source units'
    exit 0
}
echo 'ok 1 - differing test macros remain local to their source units'
exit 0
