#!/bin/sh
# Active P3 snap promotes byte input to float work.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=palette_contract ${SIXEL_RUNTIME-} \
    "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    -Lbuiltin:cms_engine=none! -.auto -Qk -Xlinear -Wlinear \
    -dnone -aoff -_nearest -Udisplay-p3 -v \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1) || {
    echo "not ok 1 - P3 snap rejected byte input"
    exit 0
}
case "$trace" in
    *'LSXSN2|final='[1-9]*) ;;
    *) echo "not ok 1 - destination snap did not execute"; exit 0 ;;
esac
case "$trace" in
    *'work=linear-f32'*) ;;
    *) echo "not ok 1 - P3 snap lost float precision"; exit 0 ;;
esac
echo "ok 1 - P3 snap promotes byte input and executes"
exit 0
