#!/bin/sh
# Policy: docs/encoder/palette-construction/snap-policy/README.md
# Policy: docs/encoder/output/output-colorspace.md
# Verify exact snap and output conversion produce SMPTE-C wire values.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v
# In linear clustering space, nearest snap maps sRGB (128,64,32) to
# safe bytes (128,64,31). Independent SMPTE-C matrix/gamma evaluation
# then rounds to (51,25,14); (51,25,15) describes the unsnapped source.
stream=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:global_budget=1 \
    --precision-policy=float32 --quality=full --sampling-policy=full-frame \
    --binning-policy=hard --quantize-model=kmeans:seed=1:binbits=6 \
    --merge-policy=none --cover-policy=off --lookup-policy=none \
    --gpu-policy=off --diffusion=none --palette-type=rgb -p2 \
    -Xsrgb-linear -Woklab -Usmpte-c --snap-policy=nearest:rate=1 \
    -Lbuiltin! --cms-engine=none - <<'PPM'
P3
1 1
255
128 64 32
PPM
) || {
    printf '\n%s\n' "not ok 1 - snap encoding failed"
    exit 0
}
case "${stream}" in
    *'#0;2;51;25;14#0@'*) ;;
    *) printf '\n%s\n' "not ok 1 - SMPTE-C wire values differ"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - exact snap uses the SMPTE-C wire grid"
exit 0
