#!/bin/sh
# Verify blue zero cielab at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0026_blue_zero_cielab || {
    echo "not ok 1 - blue_zero_cielab"
    exit 0
}

echo "ok 1 - blue_zero_cielab"
exit 0
