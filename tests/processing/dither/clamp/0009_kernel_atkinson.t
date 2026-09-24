#!/bin/sh
# Verify kernel atkinson using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0009_kernel_atkinson" || {
    echo "not ok 1 - kernel_atkinson"
    exit 0
}
echo "ok 1 - kernel_atkinson"
exit 0
