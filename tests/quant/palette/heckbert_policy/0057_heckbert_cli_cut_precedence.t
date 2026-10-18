#!/bin/sh
# Observe effective Heckbert build settings for cli cut precedence.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -=1 -Goff -p32 --env SIXEL_PALETTE_HECKBERT_CUT=sse -Qheckbert:cut=median \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - cli_cut_precedence conversion"
    exit 0
}
case "$trace" in *'LSXHBC1|rep=2|cut=median|order=sse|weights=0/0/0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - cli_cut_precedence effective settings"
    exit 0
}
echo "ok 1 - cli_cut_precedence"
exit 0
