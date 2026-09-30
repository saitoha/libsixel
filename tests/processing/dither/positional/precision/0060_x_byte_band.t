#!/bin/sh
# Verify byte saturation and absolute coordinates in overlapped bands.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0060_x_byte_band || {
    echo "not ok 1 - x byte band coordinates"
    exit 0
}

echo "ok 1 - x byte band coordinates"
exit 0
