#!/bin/sh
# Verify blue bias gamma at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0028_blue_bias_gamma || {
    printf '\n%s\n' "not ok 1 - blue_bias_gamma"
    exit 0
}

printf '\n%s\n' "ok 1 - blue_bias_gamma"
exit 0
