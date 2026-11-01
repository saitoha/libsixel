#!/bin/sh
# Verify gray sierra3 using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0018_carry_gray_sierra3" || {
    echo "not ok 1 - gray_sierra3"
    exit 0
}
echo "ok 1 - gray_sierra3"
exit 0
