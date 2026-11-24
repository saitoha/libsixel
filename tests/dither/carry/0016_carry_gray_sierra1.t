#!/bin/sh
# Verify gray sierra1 using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0016_carry_gray_sierra1" || {
    printf '\n%s\n' "not ok 1 - gray_sierra1"
    exit 0
}
printf '\n%s\n' "ok 1 - gray_sierra1"
exit 0
