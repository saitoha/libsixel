#!/bin/sh
# Verify blue gradient endpoints at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0034_blue_gradient_endpoints || {
    echo "not ok 1 - blue_gradient_endpoints"
    exit 0
}

echo "ok 1 - blue_gradient_endpoints"
exit 0
