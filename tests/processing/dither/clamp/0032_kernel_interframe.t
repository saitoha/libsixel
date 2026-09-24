#!/bin/sh
# Verify kernel interframe using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0032_kernel_interframe" || {
    echo "not ok 1 - kernel_interframe"
    exit 0
}
echo "ok 1 - kernel_interframe"
exit 0
