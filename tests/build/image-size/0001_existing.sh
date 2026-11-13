#!/bin/sh
# Verify an unchanged existing oversized use stays scoped.
set -eux
echo '1..1'
set -v
src_root=$1
awk -v control=existing \
    -f "$src_root/tests/_static/awk/sha256.awk" \
    -f "$src_root/tests/_static/awk/test-image-size.awk" /dev/null || {
    echo 'not ok 1 - an unchanged existing oversized use stays scoped'
    exit 0
}
echo 'ok 1 - an unchanged existing oversized use stays scoped'
exit 0
