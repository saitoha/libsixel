#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify WebP delay, frame, loop, and multiframe metadata exactly.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0129_loader_builtin_webp_animation_metadata_numeric" || {
    echo "not ok 1 - WebP animation metadata"
    exit 0
}

echo "ok 1 - WebP animation metadata is exact"
exit 0
