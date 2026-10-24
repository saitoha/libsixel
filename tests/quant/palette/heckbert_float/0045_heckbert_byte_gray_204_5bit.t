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

sixel=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -=greedy:B1 -Goff --lookup-policy=5bit <<'PNM'
P3
1 1
255
204 204 204
PNM
) || {
    echo "not ok 1 - uniform gray 204 with 5bit"
    exit 0
}
case "$sixel" in *'#0;2;80;80;80#0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - gray 204 must emit 80 percent with 5bit"
    exit 0
}

echo "ok 1 - gray 204 emits 80 percent with 5bit"
exit 0
