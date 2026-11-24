#!/bin/sh
# Verify image-map clustering: linear mean.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0001_map_linear_mean \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    printf '\n%s\n' "not ok 1 - map linear mean"
    exit 0
}
printf '\n%s\n' "ok 1 - map linear mean"
exit 0
