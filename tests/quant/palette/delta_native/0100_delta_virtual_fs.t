#!/bin/sh
# Paint the chosen FS color when virtual delta history is worse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0100_delta_virtual_fs || {
    echo "not ok 1 - virtual fs"
    exit 0
}
echo "ok 1 - virtual fs"
exit 0
