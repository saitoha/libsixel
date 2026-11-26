#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Check keep and repaint decisions outside the output threshold.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/working-space/0185_icc_work_delta_threshold \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" || {
    printf '\n%s\n' 'not ok 1 - ICC delta compares history and candidate outside the threshold'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC delta compares history and candidate outside the threshold'
exit 0
