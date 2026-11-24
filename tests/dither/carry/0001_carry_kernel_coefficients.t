#!/bin/sh
# Verify kernel coefficients using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0001_carry_kernel_coefficients" || {
    printf '\n%s\n' "not ok 1 - kernel_coefficients"
    exit 0
}
printf '\n%s\n' "ok 1 - kernel_coefficients"
exit 0
