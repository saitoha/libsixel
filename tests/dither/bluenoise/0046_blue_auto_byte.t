#!/bin/sh
# Verify blue auto byte across palette changes and reserved entries.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

SIXEL_DITHER_A_DITHER_STRENGTH=invalid \
SIXEL_DITHER_X_DITHER_STRENGTH=invalid \
SIXEL_DITHER_BLUENOISE_STRENGTH=invalid \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0070_blue_auto_byte || {
    echo "not ok 1 - blue auto byte"
    exit 0
}

echo "ok 1 - blue auto byte"
exit 0
