#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify unsnapped working colors export before the selected wire conversion.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v
gamma=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --precision-policy=float32 --quality=full \
    --sampling-policy=full-frame --binning-policy=hard \
    --quantize-model=kmeans:seed=1:binbits=6 --merge-policy=none \
    --cover-policy=off --lookup-policy=none --gpu-policy=off \
    --diffusion=none --palette-type=rgb -p8 -Xoklab -Wgamma \
    --snap-policy=none -Mpal:- -o/dev/null -Ugamma \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - gamma output palette export failed"
    exit 0
}
linear=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --precision-policy=float32 --quality=full \
    --sampling-policy=full-frame --binning-policy=hard \
    --quantize-model=kmeans:seed=1:binbits=6 --merge-policy=none \
    --cover-policy=off --lookup-policy=none --gpu-policy=off \
    --diffusion=none --palette-type=rgb -p8 -Xoklab -Wgamma \
    --snap-policy=none -Mpal:- -o/dev/null -Ulinear \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - linear output palette export failed"
    exit 0
}
test "${gamma}" = "${linear}" || {
    echo "not ok 1 - wire conversion changed the exported palette"
    exit 0
}
echo "ok 1 - unsnapped export is independent of wire conversion"
exit 0
