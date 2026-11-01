#!/bin/sh
# Verify x zero oklab at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0014_x_zero_oklab || {
    echo "not ok 1 - x_zero_oklab"
    exit 0
}

echo "ok 1 - x_zero_oklab"
exit 0
