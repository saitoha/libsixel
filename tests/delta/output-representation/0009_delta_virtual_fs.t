#!/bin/sh
# Paint the chosen FS color when virtual delta history is worse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0100_delta_virtual_fs || {
    printf '\n%s\n' "not ok 1 - virtual fs"
    exit 0
}
printf '\n%s\n' "ok 1 - virtual fs"
exit 0
