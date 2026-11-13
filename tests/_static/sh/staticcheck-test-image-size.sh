#!/bin/sh
# Bound literal raster fixtures; existing oversized uses cannot spread.
# Policy: docs/testing/guide.md
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
inventory=$(mktemp "${TMPDIR:-/tmp}/libsixel-image-size-XXXXXX")
trap 'rm -f "$inventory"' EXIT HUP INT TERM
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' --name '*.c' --name '*.h' --name '*.sh' \
    --name '*.py' --name '*.rb' --name '*.pl' --name '*.php' \
    --exclude-path '*/data/*' --exclude-path '*/_static/*' \
    > "$inventory" || {
    echo 'not ok 1 - cannot enumerate image fixture references'
    exit 0
}
awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/sha256.awk" \
    -f "$src_root/tests/_static/awk/test-image-size.awk" "$inventory" || {
    echo 'not ok 1 - image fixtures stay within their size budget'
    exit 0
}
echo 'ok 1 - image fixtures stay within their size budget'
exit 0
