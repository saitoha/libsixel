#!/bin/sh
# Verify a absolute band at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0009_a_absolute_band || {
    echo "not ok 1 - a_absolute_band"
    exit 0
}

echo "ok 1 - a_absolute_band"
exit 0
