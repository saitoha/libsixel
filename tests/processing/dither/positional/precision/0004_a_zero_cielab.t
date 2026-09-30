#!/bin/sh
# Verify a zero cielab at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0004_a_zero_cielab || {
    echo "not ok 1 - a_zero_cielab"
    exit 0
}

echo "ok 1 - a_zero_cielab"
exit 0
