#!/bin/sh
# Verify image-map clustering: structured byte.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0011_map_structured_byte \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" || {
    echo "not ok 1 - map structured byte"
    exit 0
}
echo "ok 1 - map structured byte"
exit 0
