#!/bin/sh
# Verify image-map clustering: din99d mean.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_cluster/0004_map_din99d_mean \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/red-blue.ppm" || {
    printf '\n%s\n' "not ok 1 - map din99d mean"
    exit 0
}
printf '\n%s\n' "ok 1 - map din99d mean"
exit 0
