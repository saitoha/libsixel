#!/bin/sh
# Verify image dimensions from header bytes, independently of the filename.
set -eux
echo '1..1'
set -v
src_root=$1
status=0
result=$(printf '%s\n' "$src_root/tests/_static/fixtures/image-size/large_header.c.in" | \
    LC_ALL=C awk -v root="$src_root" -v legacy_file=/dev/null \
        -f "$src_root/tests/_static/awk/sha256.awk" \
        -f "$src_root/tests/_static/awk/test-image-size.awk" 2>&1) || status=$?
test "$status" -eq 1 || {
    printf '%s\n' "$result" >&2
    echo 'not ok 1 - large header'
    exit 0
}
case "$result" in
    *"is 128x96; use a fixture at most 64x64"*) ;;
    *)
        printf '%s\n' "$result" >&2
        echo 'not ok 1 - large header'
        exit 0
        ;;
esac
echo 'ok 1 - large header'
exit 0
