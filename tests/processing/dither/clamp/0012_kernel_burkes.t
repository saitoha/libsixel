#!/bin/sh
# Verify kernel burkes using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0012_kernel_burkes" || {
    echo "not ok 1 - kernel_burkes"
    exit 0
}
echo "ok 1 - kernel_burkes"
exit 0
