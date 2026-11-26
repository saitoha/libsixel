#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify the next painted color after keeping a pixel under diffuse and skip.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/working-space/0182_icc_work_delta_fs \
    "icc:${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" || {
    printf '\n%s\n' 'not ok 1 - ICC delta diffuse and skip paint different colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - ICC delta diffuse and skip paint different colors'
exit 0
