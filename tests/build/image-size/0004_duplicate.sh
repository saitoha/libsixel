#!/bin/sh
# Verify a duplicate use cannot inherit a size exception.
set -eux
echo '1..1'
set -v
src_root=$1
awk -v control=duplicate \
    -f "$src_root/tests/_static/awk/sha256.awk" \
    -f "$src_root/tests/_static/awk/test-image-size.awk" /dev/null || {
    echo 'not ok 1 - a duplicate use cannot inherit a size exception'
    exit 0
}
echo 'ok 1 - a duplicate use cannot inherit a size exception'
exit 0
