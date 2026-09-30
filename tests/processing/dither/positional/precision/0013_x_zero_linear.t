#!/bin/sh
# Verify x zero linear at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0013_x_zero_linear || {
    echo "not ok 1 - x_zero_linear"
    exit 0
}

echo "ok 1 - x_zero_linear"
exit 0
