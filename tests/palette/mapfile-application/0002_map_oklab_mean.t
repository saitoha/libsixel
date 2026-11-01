#!/bin/sh
# Verify image-map clustering: oklab mean.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0002_map_oklab_mean \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    echo "not ok 1 - map oklab mean"
    exit 0
}
echo "ok 1 - map oklab mean"
exit 0
