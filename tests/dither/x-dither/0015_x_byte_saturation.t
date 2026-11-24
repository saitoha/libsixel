#!/bin/sh
# Verify x byte saturation at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0019_x_byte_saturation || {
    printf '\n%s\n' "not ok 1 - x_byte_saturation"
    exit 0
}

printf '\n%s\n' "ok 1 - x_byte_saturation"
exit 0
