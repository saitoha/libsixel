#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP wrapper that dispatches to the unified C test runner.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_TEST_BUILTIN_PIXELFORMAT_BASELINE=1 \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat"
