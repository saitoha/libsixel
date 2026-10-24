#!/bin/sh
# A uniform byte sample must retain round(v * 100 / 255) on the wire.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

sixel=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -=balanced:B1 -Goff <<'PNM'
P3
1 1
255
255 255 255
PNM
) || {
    echo "not ok 1 - uniform gray 255 with auto"
    exit 0
}
case "$sixel" in *'#0;2;100;100;100#0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - gray 255 must emit 100 percent with auto"
    exit 0
}

echo "ok 1 - gray 255 emits 100 percent with auto"
exit 0
