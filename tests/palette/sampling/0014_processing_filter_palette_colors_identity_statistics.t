#!/bin/sh
# Verify palette colors identity statistics in the encoder branch.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v
# Fix both sample storage and coordinates for the identity branch.
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --loaders=builtin! --precision-policy=8bit \
    -Q kmeans -W srgb -X srgb -5 auto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || {
    printf '\n%s\n' "not ok 1 - palette colors conversion failed"
    exit 0
}
case "${trace}" in *'LSXBIN1|requested=auto|'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - binning request trace is missing"
    exit 0
}
case "${trace}" in *'LSXPCS1|convert=0|statistics=1|'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - auto identity omitted axis statistics"
    exit 0
}

printf '\n%s\n' "ok 1 - palette colors identity statistics"
exit 0
