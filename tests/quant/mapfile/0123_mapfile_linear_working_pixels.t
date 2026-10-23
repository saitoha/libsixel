#!/bin/sh
# Verify sRGB mapfile entries select and emit the matching source pixel in -Wlinear.
# Policy: docs/encoder/palette-construction/external-palettes.md
# Test-plan: docs/testing/mapfile-parser-coverage.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

stream=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -Lbuiltin! --cms-engine=none -Wlinear -Ugamma '-~none' -dnone \
    -m "${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-two-colors-lf.pal" - <<'PPM'
P3
1 1
255
180 180 180
PPM
) || {
    echo "not ok 1 - linear working mapfile encode failed"
    exit 0
}
# In linear light, gray 180 is closer to palette entry one (squared
# distances about 0.117 and 0.573). Interpreting palette bytes as linear
# incorrectly chooses entry zero. Wire definitions remain sRGB percentages.
case "${stream}" in
    *'#0;2;5;13;22#1;2;78;82;86#1@'*) ;;
    *) echo "not ok 1 - mapfile was reinterpreted as linear RGB"; exit 0 ;;
esac
echo "ok 1 - sRGB mapfile applies the expected pixel in linear workspace"
exit 0
