#!/bin/sh
# Verify soft direction using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0005_carry_soft_direction" || {
    printf '\n%s\n' "not ok 1 - soft_direction"
    exit 0
}
printf '\n%s\n' "ok 1 - soft_direction"
exit 0
