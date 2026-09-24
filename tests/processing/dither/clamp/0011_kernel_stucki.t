#!/bin/sh
# Verify kernel stucki using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0011_kernel_stucki" || {
    echo "not ok 1 - kernel_stucki"
    exit 0
}
echo "ok 1 - kernel_stucki"
exit 0
