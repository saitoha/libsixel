#!/bin/sh
# Verify x reprepare at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0022_x_reprepare || {
    echo "not ok 1 - x_reprepare"
    exit 0
}

echo "ok 1 - x_reprepare"
exit 0
