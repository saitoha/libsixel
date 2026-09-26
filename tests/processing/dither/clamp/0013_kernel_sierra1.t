#!/bin/sh
# Verify kernel sierra1 using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0013_kernel_sierra1" || {
    echo "not ok 1 - kernel_sierra1"
    exit 0
}
echo "ok 1 - kernel_sierra1"
exit 0
