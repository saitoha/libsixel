#!/bin/sh
# FS vertical taps propagate the limited residual in raster order.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0041_fs_vertical_residual" || {
    echo "not ok 1 - fs_vertical_residual"
    exit 0
}
echo "ok 1 - fs_vertical_residual"
exit 0
