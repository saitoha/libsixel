#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify APNG BACKGROUND disposal matches independent linear values.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0139_loader_builtin_apng_dispose_background_numeric" 1>&2 || {
    echo "not ok" 1 - "APNG BACKGROUND disposal matches independent linear values"
    exit 0
}

echo "ok" 1 - "APNG BACKGROUND disposal matches independent linear values"

exit 0
