#!/bin/sh
# Verify kernel lso2 using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0016_kernel_lso2" || {
    echo "not ok 1 - kernel_lso2"
    exit 0
}
echo "ok 1 - kernel_lso2"
exit 0
