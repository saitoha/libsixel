#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify a late APNG frame-count mismatch remains an animation error.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0180_loader_builtin_apng_late_frame_count_reject" 1>&2 || {
    echo "not ok" 1 - "late APNG frame-count mismatch was hidden"
    exit 0
}

echo "ok" 1 - "late APNG frame-count mismatch stays visible"
exit 0
