#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# The PAL interchange file carries sRGB8 even when output targets Display P3.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Udisplay-p3 -_none -~none -Fnone -trgb \
    -Qheckbert:cut=median -fnorm -saverage -p2 \
    "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    -Wlinear -aoff -dnone --binning-policy=none --sampling-policy=full-frame \
    -Mpal:- -o/dev/null "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-samples.ppm") || {
    printf '\n%s\n' 'not ok 1 - ICC palette export'
    exit 0
}
# Compare the same two RGB8 entries in either order.
{
    IFS= read -r magic
    IFS= read -r version
    IFS= read -r count
    IFS= read -r first
    IFS= read -r second
} < "${TOP_SRCDIR}/tests/data/color-numeric/icc-heckbert-srgb8.pal" || {
    printf '\n%s\n' 'not ok 1 - read expected sRGB8 palette'
    exit 0
}
actual=$(printf '%s' "$actual" | tr -d '\015')
cr=$(printf '\r')
header="${magic%"$cr"}
${version%"$cr"}
${count%"$cr"}"
first=${first%"$cr"}
second=${second%"$cr"}
test "$actual" = "$header
$first
$second" || test "$actual" = "$header
$second
$first" || {
    printf '%s\n' "$actual" >&2
    printf '\n%s\n' 'not ok 1 - exported ICC representatives differ from sRGB8'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC representatives exported in sRGB8'
exit 0
