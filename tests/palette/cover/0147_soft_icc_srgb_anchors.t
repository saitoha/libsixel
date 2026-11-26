#!/bin/sh
# Policy: docs/encoder/palette-construction/cover-policy/README.md
# Verify sRGB anchors outside a narrow clustering profile's gamut.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qkmeans:seed=1 -p2 -Wlinear -afaces:Wsoft:V1 -dnone \
    "-Xicc:${TOP_SRCDIR}/tests/data/color-numeric/narrow-p3-cluster.icc" \
    --binning-policy=none --sampling-policy=full-frame \
    -Mpal:- -o/dev/null - <<'PNM'
P3
6 1
255
255 0 0  255 0 0  128 128 128  128 128 128  0 0 255  0 0 255
PNM
) || {
    printf '\n%s\n' 'not ok 1 - soft ICC cover encode'
    exit 0
}
in_palette="
$(printf '%s' "$actual" | tr -d '\015')
"
nl='
'
case "$in_palette" in *"${nl}255 0 0$nl"*) true ;; *) false ;; esac || {
    printf '%s\n' "$actual" >&2
    printf '\n%s\n' 'not ok 1 - missing soft sRGB anchor 255 0 0'
    exit 0
}
case "$in_palette" in *"${nl}128 128 128$nl"*) true ;; *) false ;; esac || {
    printf '%s\n' "$actual" >&2
    printf '\n%s\n' 'not ok 1 - missing soft sRGB anchor 128 128 128'
    exit 0
}
case "$in_palette" in *"${nl}0 0 255$nl"*) true ;; *) false ;; esac || {
    printf '%s\n' "$actual" >&2
    printf '\n%s\n' 'not ok 1 - missing soft sRGB anchor 0 0 255'
    exit 0
}
printf '\n%s\n' 'ok 1 - soft ICC cover retains sRGB anchors'
exit 0
