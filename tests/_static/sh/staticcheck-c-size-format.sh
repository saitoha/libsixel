#!/bin/sh
# Keep size_t format conversions compatible with the configured CRT.
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
sh "$src_root/tests/_static/sh/list-source-files.sh" \
    "$src_root/src" "$src_root/converters" "$src_root/assessment" \
    --name '*.c' --name '*.h' --name '*.m' \
    --exclude-path '*/third_party/*' \
    --exec awk -f "$src_root/tests/_static/awk/c-size-format.awk" >&2 || {
    echo 'not ok 1 - production code uses configured size_t formats'
    exit 0
}
echo 'ok 1 - production code uses configured size_t formats'
exit 0
