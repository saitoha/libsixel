#!/bin/sh
# Verify gray burkes using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0015_carry_gray_burkes" || {
    echo "not ok 1 - gray_burkes"
    exit 0
}
echo "ok 1 - gray_burkes"
exit 0
