#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Snap the repaired sRGB green anchor through the Display P3 tone grid.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Udisplay-p3 -_reversible:timing=apply -~none -Fnone -trgb \
    -Qkmeans:seed=1 -p2 -Wsrgb-linear -afaces:Whard:V1 -dnone \
    "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/narrow-p3-cluster.icc" \
    --binning-policy=none --sampling-policy=full-frame -Mpal:- -o/dev/null \
    "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-samples.ppm" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - ICC cover and apply snap'
    exit 0
}
# The original sRGB green maps to P3 RGB8 (117,251,76). The reversible
# RGB101 code (46,98,30) decodes to (117,250,77), hence sRGB8 (9,254,9).
trace=$(printf '%s' "$trace" | tr -d '\015')
case "$trace" in
    *'|sample=0|apply='[1-9]*"
9 254 9
"*) ;;
    *) printf '\n%s\n' 'not ok 1 - apply snap did not retain the expected green'; exit 0 ;;
esac
printf '\n%s\n' 'ok 1 - apply snap retains the expected green'
exit 0
