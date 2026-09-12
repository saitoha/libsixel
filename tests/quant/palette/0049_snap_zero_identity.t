#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# A zero approach rate must preserve the snap-disabled typed pipeline.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v
set -- --threads=1 --precision=float32 --quality=full \
    --sampling-policy=full-frame --binning-policy=hard \
    --quantize-model=kmeans:seed=1:binbits=6 --merge-policy=none \
    --cover-policy=off --lookup-policy=none --gpu-policy=off \
    --diffusion=none -Xdin99d -Woklab -p8 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --snap-policy=none "$@") || {
    echo "not ok 1 - reference encoding failed"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --snap-policy=nearest:timing=all:rate=0 "$@") || {
    echo "not ok 1 - zero-rate encoding failed"
    exit 0
}
test "${actual}" = "${reference}" || {
    echo "not ok 1 - zero rate changed the output"
    exit 0
}
echo "ok 1 - zero rate preserves snap-disabled output"
exit 0
