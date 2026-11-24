#!/bin/sh
# Verify p3 oklab image map.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0055_p3_mapfile image-linear \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" oklab || {
    printf '\n%s\n' "not ok 1 - p3 oklab image map"
    exit 0
}
printf '\n%s\n' "ok 1 - p3 oklab image map"
exit 0
