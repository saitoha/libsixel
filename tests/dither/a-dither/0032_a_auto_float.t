#!/bin/sh
# Verify a auto float across palette changes and reserved entries.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

SIXEL_DITHER_A_DITHER_STRENGTH=invalid \
SIXEL_DITHER_X_DITHER_STRENGTH=invalid \
SIXEL_DITHER_BLUENOISE_STRENGTH=invalid \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0065_a_auto_float || {
    echo "not ok 1 - a auto float"
    exit 0
}

echo "ok 1 - a auto float"
exit 0
