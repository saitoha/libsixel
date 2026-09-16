#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify cancellation before decode and immediately after the first WebP frame.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0163_loader_builtin_webp_cancel_boundaries" 1>&2 || {
    echo "not ok" 1 - "builtin WebP animation cancellation boundaries"
    exit 0
}
echo "ok" 1 - "builtin WebP animation cancellation boundaries"
exit 0
