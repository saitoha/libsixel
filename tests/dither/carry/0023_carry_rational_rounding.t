#!/bin/sh
# Compare carry arithmetic with the independent rational reference.
# Policy: docs/encoder/palette-application/clamp-policy.md
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0023_carry_rational_rounding" || {
    echo "not ok 1 - rounding_reference"
    exit 0
}
echo "ok 1 - rounding_reference"
exit 0
