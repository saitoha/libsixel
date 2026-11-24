#!/bin/sh
# Verify x large coordinates at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0021_x_large_coordinates || {
    printf '\n%s\n' "not ok 1 - x_large_coordinates"
    exit 0
}

printf '\n%s\n' "ok 1 - x_large_coordinates"
exit 0
