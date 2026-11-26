#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Both sRGB primaries are outside the narrow clustering profile.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
palette=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qkmeans:seed=1 -p2 "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/narrow-p3-cluster.icc" \
    -Wlinear -aoff -dnone --binning-policy=none --sampling-policy=full-frame \
    -Mpal:- -o/dev/null <<'PNM'
P3
2 1
255
255 0 0  0 0 255
PNM
) || {
    printf '\n%s\n' 'not ok 1 - extended ICC clustering encode'
    exit 0
}
palette=$(printf '%s\n' "$palette" | tr -d '\015')
test "$palette" = 'JASC-PAL
0100
2
255 0 0
0 0 255' || test "$palette" = 'JASC-PAL
0100
2
0 0 255
255 0 0' || {
    printf '%s\n' "$palette" >&2
    printf '\n%s\n' 'not ok 1 - ICC clustering preserves out-of-gamut colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC clustering preserves out-of-gamut colors'
exit 0
