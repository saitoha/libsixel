#!/bin/sh
# Verify blue reprepare at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0033_blue_reprepare || {
    echo "not ok 1 - blue_reprepare"
    exit 0
}

echo "ok 1 - blue_reprepare"
exit 0
