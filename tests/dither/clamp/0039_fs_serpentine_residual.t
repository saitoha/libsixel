#!/bin/sh
# FS reverses horizontal taps while propagating limited residuals in serpentine order.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0039_fs_serpentine_residual" || {
    echo "not ok 1 - fs_serpentine_residual"
    exit 0
}
echo "ok 1 - fs_serpentine_residual"
exit 0
