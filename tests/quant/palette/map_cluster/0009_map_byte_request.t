#!/bin/sh
# Verify precision-policy=8bit rounds an image-map clustering palette.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0009_map_byte_request \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    echo "not ok 1 - map byte request"
    exit 0
}
echo "ok 1 - map byte request"
exit 0
