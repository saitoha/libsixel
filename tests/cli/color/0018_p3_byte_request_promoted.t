#!/bin/sh
# P3 itself requires float; zero-rate snap is an identity, not a byte fallback.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v

trace=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Lbuiltin:cms_engine=none! -.8bit -Qk -Xgamma -Wgamma \
    -0hard -dnone -aoff -_auto:rate=0 -Udisplay-p3 -v \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1) || {
    echo "not ok 1 - P3 rejected a byte precision request"
    exit 0
}
case "$trace" in
    *'formats: source=rgb888 work=rgb-f32 scale_out=rgb888'*) ;;
    *) echo "not ok 1 - P3 did not promote byte input"; exit 0 ;;
esac
echo "ok 1 - P3 promotes byte input even with a byte request"
exit 0
