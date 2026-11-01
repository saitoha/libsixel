#!/bin/sh
# Verify gray fs using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0011_carry_gray_fs" || {
    echo "not ok 1 - gray_fs"
    exit 0
}
echo "ok 1 - gray_fs"
exit 0
