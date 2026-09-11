#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Fix the byte-exact output of the lossy VP8 decoder.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0112_loader_builtin_webp_vp8_digest" || {
    echo "not ok 1 - WebP VP8 exact RGB digest"
    exit 0
}

echo "ok 1 - WebP VP8 exact RGB digest"
exit 0
