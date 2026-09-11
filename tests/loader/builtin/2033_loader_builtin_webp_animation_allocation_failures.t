#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify WebP animation allocation failures clean up partial traversal.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0167_loader_builtin_webp_animation_allocation_failures" 1>&2 || {
    echo "not ok" 1 - "WebP animation allocation failure cleanup"
    exit 0
}
echo "ok" 1 - "WebP animation allocation failure cleanup"
exit 0
