#!/bin/sh
# Verify x bias gamma at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0017_x_bias_gamma || {
    echo "not ok 1 - x_bias_gamma"
    exit 0
}

echo "ok 1 - x_bias_gamma"
exit 0
