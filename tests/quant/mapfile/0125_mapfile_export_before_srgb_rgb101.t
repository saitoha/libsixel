#!/bin/sh
# Verify sRGB output exports pre-wire sRGB8 palette values.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
# Test-plan: docs/testing/mapfile-parser-coverage.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

# This source is not on the RGB101 grid. Export after inverse projection of
# the sRGB wire palette changes it; the interchange boundary must precede that.
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    -Lbuiltin! --cms-engine=none --precision-policy=float32 \
    --sampling-policy=full-frame --binning-policy=none \
    -Qkmeans:seed=1 -Xlinear -Wlinear -Ugamma \
    -Fnone -aoff -_none '-~none' -dnone -trgb -Mpal:- -o/dev/null - <<'PPM'
P3
1 1
255
129 65 33
PPM
) || {
    echo "not ok 1 - sRGB palette export failed"
    exit 0
}
actual=$(printf '%s' "${actual}" | tr -d '\015')
test "${actual}" = 'JASC-PAL
0100
1
129 65 33' || {
    echo "not ok 1 - device values or RGB101 rounding leaked into export"
    exit 0
}
echo "ok 1 - sRGB exports pre-wire sRGB palette"
exit 0
