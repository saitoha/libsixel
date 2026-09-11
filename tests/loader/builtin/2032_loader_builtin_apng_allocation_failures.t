#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify APNG allocation failures cannot become truncated success.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0166_loader_builtin_apng_allocation_failures" 1>&2 || {
    echo "not ok" 1 - "APNG allocation failures stay failures"
    exit 0
}
echo "ok" 1 - "APNG allocation failures stay failures"
exit 0
