#!/bin/sh
# Verify reset colorspace auto at planner and encoder boundaries.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/0004_reset_colorspace_auto 1>&2 || {
    echo "not ok 1 - reset colorspace auto"
    exit 0
}
echo "ok 1 - reset colorspace auto"
exit 0
