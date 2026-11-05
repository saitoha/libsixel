#!/bin/sh
# Included production C definitions must not collide with the linked library.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
LC_ALL=C
export LC_ALL
inventory=$(mktemp "${TMPDIR:-/tmp}/sixel-source-isolation-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
sh "$1/tests/_static/sh/list-source-files.sh" "$1/tests" \
    --name '*.[ch]' > "$inventory"
awk -v src_root="$1" -v manual_scan=1 \
    -f "$1/tests/_static/awk/dllexport-declarations.awk" \
    -f "$1/tests/_static/awk/test-source-isolation.awk" \
    "$inventory" || {
    echo 'not ok 1 - included production functions have private test names'
    exit 0
}
echo 'ok 1 - included production functions have private test names'
exit 0
