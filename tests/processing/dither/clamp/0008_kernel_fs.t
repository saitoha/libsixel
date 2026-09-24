#!/bin/sh
# Verify kernel fs using fixed numeric expectations.
# Policy: docs/functionality/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0008_kernel_fs" || {
    echo "not ok 1 - kernel_fs"
    exit 0
}
echo "ok 1 - kernel_fs"
exit 0
