#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Preserve the sRGB8 palette export before ICC register projection.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    --sampling-policy=full-frame --binning-policy=none \
    -7 -Lbuiltin! -Qkmeans:seed=1 -Xoklab -Wsrgb-linear \
    -U "icc:P${TOP_SRCDIR}/tests/data/color-numeric/output-bpc.icc" \
    -_none -~none -Fnone -aoff -dnone -trgb -Mpal:- -o/dev/null - <<'PPM'
P3
1 1
65535
51992 61688 54940
PPM
) || {
    printf '\n%s\n' 'not ok 1 - explicit ICC palette export'
    exit 0
}
actual=$(printf '%s' "$actual" | tr -d '\015')
test "$actual" = 'JASC-PAL
0100
1
202 240 214' || {
    printf '\n%s\n' 'not ok 1 - export preserves original sRGB8 colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - export preserves original sRGB8 colors'
exit 0
