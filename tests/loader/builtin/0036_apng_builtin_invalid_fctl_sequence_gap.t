#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify a sequence error after one decoded frame cannot fall back to static.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0181_loader_builtin_apng_late_sequence_reject" 1>&2 || {
    echo "not ok" 1 - "late APNG sequence error was hidden"
    exit 0
}

echo "ok" 1 - "late APNG sequence error stays visible"
exit 0
