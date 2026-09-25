#!/bin/sh
# Verify P3 fixed-palette act values and pixel application.
# Policy: docs/functionality/output-colorspace.md
# Policy: docs/functionality/external-palettes.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0055_p3_mapfile linear \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.act" || {
    echo "not ok 1 - P3 mapfile act"
    exit 0
}
echo "ok 1 - P3 mapfile act"
exit 0
