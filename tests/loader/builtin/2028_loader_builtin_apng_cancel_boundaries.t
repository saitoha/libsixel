#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify cancellation before decode and immediately after the first APNG frame.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0162_loader_builtin_apng_cancel_boundaries" 1>&2 || {
    echo "not ok" 1 - "builtin APNG cancellation boundaries"
    exit 0
}
echo "ok" 1 - "builtin APNG cancellation boundaries"
exit 0
