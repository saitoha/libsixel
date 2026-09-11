#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify VP8L transform reversal with an exact decoded RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0097_loader_builtin_webp_vp8l_transform_rgb_digest" || {
    echo "not ok 1 - WebP VP8L exact decoded RGB digest"
    exit 0
}

echo "ok 1 - WebP VP8L exact decoded RGB digest"
exit 0
