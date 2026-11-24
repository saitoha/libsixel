#!/bin/sh
# Verify kernel fs using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0008_kernel_fs" || {
    printf '\n%s\n' "not ok 1 - kernel_fs"
    exit 0
}
printf '\n%s\n' "ok 1 - kernel_fs"
exit 0
