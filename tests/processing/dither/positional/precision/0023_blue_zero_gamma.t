#!/bin/sh
# Verify blue zero gamma at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0023_blue_zero_gamma || {
    echo "not ok 1 - blue_zero_gamma"
    exit 0
}

echo "ok 1 - blue_zero_gamma"
exit 0
