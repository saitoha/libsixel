#!/bin/sh
# Verify palette colors hard conversion only in the encoder branch.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" -Q kmeans -X oklab -5 hard \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - palette colors conversion failed"
    exit 0
}
case "${trace}" in *'LSXBIN1|requested=hard|'*) true ;; *) false ;; esac || {
    echo "not ok 1 - binning request trace is missing"
    exit 0
}
case "${trace}" in *'LSXPCS1|convert=1|statistics=0|'*) true ;; *) false ;; esac || {
    echo "not ok 1 - hard colorspace transform did not use its filter"
    exit 0
}
case "${trace}" in *'statistics=1|'*) false ;; *) true ;; esac || {
    echo "not ok 1 - hard colorspace transform collected unused statistics"
    exit 0
}

echo "ok 1 - palette colors hard conversion only"
exit 0
