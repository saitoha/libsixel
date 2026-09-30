#!/bin/sh
# Verify blue zero oklab at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0025_blue_zero_oklab || {
    echo "not ok 1 - blue_zero_oklab"
    exit 0
}

echo "ok 1 - blue_zero_oklab"
exit 0
