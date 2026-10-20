#!/bin/sh
# Compare candidate arithmetic with the frozen double reference.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0024_carry_bounds_reference" || {
    echo "not ok 1 - bounds_reference"
    exit 0
}
echo "ok 1 - bounds_reference"
exit 0
