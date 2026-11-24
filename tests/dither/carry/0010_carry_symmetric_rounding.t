#!/bin/sh
# Verify symmetric rounding using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0010_carry_symmetric_rounding" || {
    printf '\n%s\n' "not ok 1 - symmetric_rounding"
    exit 0
}
printf '\n%s\n' "ok 1 - symmetric_rounding"
exit 0
