#!/bin/sh
# Check macro collisions and fallback ordering without a target compiler.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" \
    "$src_root/src" "$src_root/include" "$src_root/converters" \
    "$src_root/assessment" "$src_root/tests" \
    --name '*.c' --name '*.h' --name '*.h.in' \
    --exclude-path '*/third_party/*' --exclude-path '*/data/*' \
    --exec awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/c-preprocessor-portability.awk" >&2 || {
    echo 'not ok 1 - C identifiers and integer fallback ordering are portable'
    exit 0
}
echo 'ok 1 - C identifiers and integer fallback ordering are portable'
exit 0
