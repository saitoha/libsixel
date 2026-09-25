#!/bin/sh
# Verify Linear RGB output exports pre-wire sRGB8 palette values.
# Policy: docs/functionality/output-colorspace.md
# Policy: docs/functionality/external-palettes.md
# Test-plan: docs/testing/mapfile-parser-coverage.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v

# Output conversion to linear RGB must not change the sRGB export bytes.
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Lbuiltin! --cms-engine=none --precision=float32 \
    --sampling-policy=full-frame --binning-policy=none \
    -Qkmeans:seed=1 -Xgamma -Wgamma -Ulinear \
    -Fnone -aoff -_none '-~none' -dnone -trgb -Mpal:- -o/dev/null - <<'PPM'
P3
1 1
255
129 65 33
PPM
) || {
    echo "not ok 1 - Linear RGB palette export failed"
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
echo "ok 1 - Linear RGB exports pre-wire sRGB palette"
exit 0
