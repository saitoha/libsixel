#!/bin/sh
# Verify blue large coordinates at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0032_blue_large_coordinates || {
    echo "not ok 1 - blue_large_coordinates"
    exit 0
}

echo "ok 1 - blue_large_coordinates"
exit 0
