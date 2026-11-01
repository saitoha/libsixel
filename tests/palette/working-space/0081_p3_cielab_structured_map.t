#!/bin/sh
# Verify p3 cielab structured map.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0055_p3_mapfile linear \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" cielab || {
    echo "not ok 1 - p3 cielab structured map"
    exit 0
}
echo "ok 1 - p3 cielab structured map"
exit 0
