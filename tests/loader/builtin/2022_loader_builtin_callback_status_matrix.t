#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin.md
# Verify builtin formats propagate callback interruption.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0156_loader_builtin_callback_status_matrix" 1>&2 || {
    echo "not ok" 1 - "builtin formats propagate callback interruption"
    exit 0
}

echo "ok" 1 - "builtin formats propagate callback interruption"

exit 0
