#!/bin/sh
# Verify blue strength override across palette changes and reserved entries.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

SIXEL_DITHER_A_DITHER_STRENGTH=0.5 \
SIXEL_DITHER_X_DITHER_STRENGTH=0.5 \
SIXEL_DITHER_BLUENOISE_STRENGTH=0.5 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0076_blue_strength_override || {
    echo "not ok 1 - blue strength override"
    exit 0
}

echo "ok 1 - blue strength override"
exit 0
