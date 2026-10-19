#!/bin/sh
# Verify gray stucki using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0014_carry_gray_stucki" || {
    echo "not ok 1 - gray_stucki"
    exit 0
}
echo "ok 1 - gray_stucki"
exit 0
