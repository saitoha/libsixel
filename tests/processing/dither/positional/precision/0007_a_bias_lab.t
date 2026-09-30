#!/bin/sh
# Verify a bias lab at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0007_a_bias_lab || {
    echo "not ok 1 - a_bias_lab"
    exit 0
}

echo "ok 1 - a_bias_lab"
exit 0
