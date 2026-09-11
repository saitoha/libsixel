#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify VP8L color-index reversal against an exact RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0171_loader_builtin_webp_vp8l_color_index_digest" 1>&2 || {
    echo "not ok" 1 - "VP8L color-index RGB digest"
    exit 0
}
echo "ok" 1 - "VP8L color-index RGB digest"
exit 0
