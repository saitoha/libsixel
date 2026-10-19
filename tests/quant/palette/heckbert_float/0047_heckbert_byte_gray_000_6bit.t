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

sixel=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -=1 -Goff --lookup-policy=6bit <<'PNM'
P3
1 1
255
0 0 0
PNM
) || {
    echo "not ok 1 - uniform gray 0 with 6bit"
    exit 0
}
case "$sixel" in *'#0;2;0;0;0#0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - gray 0 must emit 0 percent with 6bit"
    exit 0
}

echo "ok 1 - gray 0 emits 0 percent with 6bit"
exit 0
