#!/bin/sh
# Verify blue strength env across palette changes and reserved entries.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

SIXEL_DITHER_A_DITHER_STRENGTH=0.5 \
SIXEL_DITHER_X_DITHER_STRENGTH=0.5 \
SIXEL_DITHER_BLUENOISE_STRENGTH=0.5 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0073_blue_strength_env || {
    echo "not ok 1 - blue strength env"
    exit 0
}

echo "ok 1 - blue strength env"
exit 0
