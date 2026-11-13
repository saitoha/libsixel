#!/bin/sh
# Verify an enlarged fixture cannot inherit a size exception.
set -eux
echo '1..1'
set -v
src_root=$1
awk -v control=larger \
    -f "$src_root/tests/_static/awk/sha256.awk" \
    -f "$src_root/tests/_static/awk/test-image-size.awk" /dev/null || {
    echo 'not ok 1 - an enlarged fixture cannot inherit a size exception'
    exit 0
}
echo 'ok 1 - an enlarged fixture cannot inherit a size exception'
exit 0
