#!/bin/sh
# Verify default colorspace auto at planner and encoder boundaries.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/0003_default_colorspace_auto 1>&2 || {
    echo "not ok 1 - default colorspace auto"
    exit 0
}
echo "ok 1 - default colorspace auto"
exit 0
