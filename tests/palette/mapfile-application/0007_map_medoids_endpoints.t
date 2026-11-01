#!/bin/sh
# Verify image-map clustering: medoids endpoints.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0007_map_medoids_endpoints \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    echo "not ok 1 - map medoids endpoints"
    exit 0
}
echo "ok 1 - map medoids endpoints"
exit 0
