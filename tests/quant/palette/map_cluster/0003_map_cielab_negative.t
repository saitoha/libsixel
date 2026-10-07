#!/bin/sh
# Verify image-map clustering: cielab negative.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0003_map_cielab_negative \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    echo "not ok 1 - map cielab negative"
    exit 0
}
echo "ok 1 - map cielab negative"
exit 0
