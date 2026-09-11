#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify alpha-bearing ANIM background composition by frame digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0172_loader_builtin_webp_alpha_background_digest" 1>&2 || {
    echo "not ok" 1 - "WebP alpha animation background digests"
    exit 0
}
echo "ok" 1 - "WebP alpha animation background digests"
exit 0
