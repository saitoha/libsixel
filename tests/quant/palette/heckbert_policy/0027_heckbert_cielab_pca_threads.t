#!/bin/sh
# cielab pca SSE palettes agree at one and eight workers.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
single=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 -Xcielab \
    -fpca -Qheckbert:Tsse:Osse -=1 -M gpl:- -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png")
parallel=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 -Xcielab \
    -fpca -Qheckbert:Tsse:Osse -=8 -M gpl:- -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png")
test -n "$single" && test "$single" = "$parallel" || {
    echo "not ok 1 - worker count changed SSE palette"
    exit 0
}
echo "ok 1 - cielab_pca_threads"
exit 0
