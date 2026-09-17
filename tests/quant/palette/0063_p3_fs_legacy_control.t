#!/bin/sh
# Verify legacy FS clamp control.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0062_p3_fs_error_contract legacy || {
    echo "not ok 1 - legacy FS clamp control"
    exit 0
}

echo "ok 1 - legacy FS clamp control"
exit 0
