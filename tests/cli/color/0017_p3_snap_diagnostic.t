#!/bin/sh
# Active P3 snap names the unsupported operation, not the precision path.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

status=0
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qk -Xl -_auto -Udisplay-p3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1) || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - unsupported snap was accepted"
    exit 0
}
case "$message" in
    *'display-p3 does not support active snap; use -_none or a zero snap rate.') ;;
    *) echo "not ok 1 - P3 snap diagnostic is misleading"; exit 0 ;;
esac
echo "ok 1 - P3 snap diagnostic names the incompatible operation"
exit 0
