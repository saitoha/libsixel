#!/bin/sh
# Verify a zero gamma at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0001_a_zero_gamma || {
    printf '\n%s\n' "not ok 1 - a_zero_gamma"
    exit 0
}

printf '\n%s\n' "ok 1 - a_zero_gamma"
exit 0
