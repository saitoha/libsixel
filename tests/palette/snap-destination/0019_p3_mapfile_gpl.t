#!/bin/sh
# Verify P3 fixed-palette gpl values and pixel application.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0055_p3_mapfile linear \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.gpl" || {
    printf '\n%s\n' "not ok 1 - P3 mapfile gpl"
    exit 0
}
printf '\n%s\n' "ok 1 - P3 mapfile gpl"
exit 0
