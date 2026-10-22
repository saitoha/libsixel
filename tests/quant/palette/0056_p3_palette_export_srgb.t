#!/bin/sh
# Verify Display P3 output exports pre-wire sRGB8 palette values.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

# This source is not on the RGB101 grid. Export must preserve its pre-wire
# sRGB values even when the separate output copy rounds to P3 wire colors.
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Lbuiltin! --cms-engine=none --precision-policy=float32 \
    --sampling-policy=full-frame --binning-policy=none \
    -Qkmeans:seed=1 -Xlinear -Wlinear -Udisplay-p3 \
    -Fnone -aoff -_none '-~none' -dnone -trgb -Mpal:- -o/dev/null - <<'PPM'
P3
1 1
255
129 65 33
PPM
) || {
    echo "not ok 1 - Display P3 palette export failed"
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
echo "ok 1 - Display P3 exports pre-wire sRGB palette"
exit 0
