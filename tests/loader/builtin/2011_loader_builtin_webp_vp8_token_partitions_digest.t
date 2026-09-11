#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify WebP VP8 token partitions produce exact RGB.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0145_loader_builtin_webp_vp8_token_partitions_digest" 1>&2 || {
    echo "not ok" 1 - "WebP VP8 token partitions produce exact RGB"
    exit 0
}

echo "ok" 1 - "WebP VP8 token partitions produce exact RGB"

exit 0
