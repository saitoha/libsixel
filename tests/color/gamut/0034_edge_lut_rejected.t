#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0034_edge_lut_rejected "icc:path=${TOP_SRCDIR}/tests/data/color-numeric/relative-paired-lut.icc:gamut=edge-seeker" || {
    printf '\n%s\n' "not ok 1 - lut rejected"
    exit 0
}
printf '\n%s\n' "ok 1 - lut rejected"
exit 0
