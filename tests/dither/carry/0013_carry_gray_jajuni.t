#!/bin/sh
# Verify gray jajuni using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0013_carry_gray_jajuni" || {
    printf '\n%s\n' "not ok 1 - gray_jajuni"
    exit 0
}
printf '\n%s\n' "ok 1 - gray_jajuni"
exit 0
