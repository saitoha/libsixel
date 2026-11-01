#!/bin/sh
# Common ratio scaling cancels and explicit weights override environment.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 -Xc \
    -Qheckbert:Tsse:Osse:W2/1/1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png")
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -p16 -Xc \
    --env SIXEL_PALETTE_HECKBERT_WEIGHTS=9/1/1 \
    -Qheckbert:Tsse:Osse:weights=4/2/2 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png")
test -n "$first" && test "$first" = "$second" || {
    echo "not ok 1 - common scale or CLI precedence changed output"
    exit 0
}
echo "ok 1 - weight_scale"
exit 0
