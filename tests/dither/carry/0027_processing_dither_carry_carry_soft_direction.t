#!/bin/sh
# Rational soft scaling preserves correction direction within rounding.
# Policy: docs/encoder/palette-application/dithering/README.md
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0027_carry_soft_direction" || {
    printf '\n%s\n' "not ok 1 - carry_soft_direction"
    exit 0
}
printf '\n%s\n' "ok 1 - carry_soft_direction"
exit 0
