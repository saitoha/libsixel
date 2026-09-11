#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin.md
# Verify builtin formats handle allocation failures atomically.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0155_loader_builtin_allocator_failure_matrix" 1>&2 || {
    echo "not ok" 1 - "builtin formats handle allocation failures atomically"
    exit 0
}

echo "ok" 1 - "builtin formats handle allocation failures atomically"

exit 0
