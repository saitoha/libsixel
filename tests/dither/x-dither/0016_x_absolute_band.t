#!/bin/sh
# Verify x absolute band at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0020_x_absolute_band || {
    echo "not ok 1 - x_absolute_band"
    exit 0
}

echo "ok 1 - x_absolute_band"
exit 0
