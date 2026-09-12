#!/bin/sh
# Policy: docs/functionality/snap-policy.md
# Verify generated exact snap agrees at ACT and SIXEL output boundaries.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

stream=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=float32 --quality=full --sampling-policy=full-frame \
    --binning-policy=hard --quantize-model=kmeans:seed=1:binbits=6 \
    --merge-policy=none --cover-policy=off --lookup-policy=none \
    --gpu-policy=off --diffusion=none --palette-type=rgb -p8 \
    -Xlinear -Woklab -Usmpte-c --snap-policy=nearest:rate=1 \
    -M "act:${ARTIFACT_LOCAL_DIR}/palette.act" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png") || {
    echo "not ok 1 - snap encoding failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/0045_snap_output_grid \
    "${ARTIFACT_LOCAL_DIR}/palette.act" "${stream}" || {
    echo "not ok 1 - ACT fixed points differ from SIXEL output"
    exit 0
}
echo "ok 1 - exact snap preserves output fixed points"
exit 0
