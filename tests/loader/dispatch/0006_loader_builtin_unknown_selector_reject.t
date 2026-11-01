#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify an unknown unified-runner selector cannot pass through the baseline.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_TEST_NONEXISTENT_SELECTOR=1 \
    "loader/0014_loader_builtin_pixelformat" && {
    echo "not ok 1 - unknown builtin loader selector was accepted"
    exit 0
}

echo "ok 1 - unknown builtin loader selector is rejected"
exit 0
