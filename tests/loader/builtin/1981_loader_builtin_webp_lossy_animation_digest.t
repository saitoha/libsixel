#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Fix exact composited frames for lossy WebP animation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0113_loader_builtin_webp_lossy_animation_digest" || {
    echo "not ok 1 - WebP lossy animation exact frame digests"
    exit 0
}

echo "ok 1 - WebP lossy animation exact frame digests"
exit 0
