#!/bin/sh
# Standard C and Objective-C require the conditional middle operand.
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
sh "$src_root/tests/_static/sh/list-source-files.sh" \
    "$src_root/src" "$src_root/include" "$src_root/converters" \
    "$src_root/assessment" "$src_root/tests" "$src_root/tools" \
    "$src_root/examples" "$src_root/fuzz" \
    --name '*.c' --name '*.h' --name '*.h.in' --name '*.m' \
    --exclude-path '*/third_party/*' --exclude-path '*/data/*' \
    --exclude-name 'stb_image.h' --exclude-name 'stb_image_write.h' \
    --exec awk -f "$src_root/tests/_static/awk/c-conditional-operand.awk" >&2 || {
    echo 'not ok 1 - conditional expressions have a standard middle operand'
    exit 0
}
echo 'ok 1 - conditional expressions have a standard middle operand'
exit 0
