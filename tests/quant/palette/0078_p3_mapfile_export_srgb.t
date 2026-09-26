#!/bin/sh
# Verify fixed-palette export precedes P3 RGB101 rounding.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel disabled"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Lbuiltin! --cms-engine=none --precision=auto \
    --sampling-policy=full-frame --binning-policy=none \
    -Qkmeans:seed=1 -Xlinear -Wgamma -Udisplay-p3 \
    -Fnone -_none '-~none' -dnone -trgb -Goff \
    -m "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" \
    -Mpal:- -o/dev/null - <<'PPM'
P3
1 1
255
91 91 91
PPM
) || {
    echo "not ok 1 - P3 fixed-palette export failed"
    exit 0
}
actual=$(printf '%s' "${actual}" | tr -d '\015')
test "${actual}" = 'JASC-PAL
0100
4
88 88 88
94 94 94
88 88 88
255 0 0' || {
    echo "not ok 1 - wire rounding, gamut conversion or compaction leaked into export"
    exit 0
}
echo "ok 1 - P3 fixed palette exports original sRGB entries"
exit 0
