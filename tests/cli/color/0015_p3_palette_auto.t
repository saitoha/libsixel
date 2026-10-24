#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify p3 palette auto emits the independent sRGB-red to P3 RGB101 value.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    exit 0
}

echo "1..1"
set -v
stream=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    -.f -Udisplay-p3 -aoff -tauto -Lbuiltin! --cms-engine=none - <<'PPM'
P3
1 1
255
255 0 0
PPM
) || {
    echo "not ok 1 - p3 palette auto rejected"
    exit 0
}
case "$stream" in
    *'#0;2;92;20;14#0@'*) ;;
    *) echo "not ok 1 - p3 palette auto wire values differ"; exit 0 ;;
esac
echo "ok 1 - p3 palette auto selects RGB output"
exit 0
