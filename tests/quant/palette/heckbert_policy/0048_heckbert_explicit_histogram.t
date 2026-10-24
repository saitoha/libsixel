#!/bin/sh
# Observe effective Heckbert build settings for explicit histogram.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -=balanced:B1 -Goff -p32 -dfs -shistogram \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - explicit_histogram conversion"
    exit 0
}
case "$trace" in *'LSXHBC1|rep=3|cut=sse|order=sse|weights=0/0/0'*) true ;; *) false ;; esac || {
    echo "not ok 1 - explicit_histogram effective settings"
    exit 0
}
echo "ok 1 - explicit_histogram"
exit 0
