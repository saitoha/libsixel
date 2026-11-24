#!/bin/sh
# Verify kernel sierra3 using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0015_kernel_sierra3" || {
    printf '\n%s\n' "not ok 1 - kernel_sierra3"
    exit 0
}
printf '\n%s\n' "ok 1 - kernel_sierra3"
exit 0
