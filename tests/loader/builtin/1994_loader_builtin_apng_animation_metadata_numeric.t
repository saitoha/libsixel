#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify APNG delay, frame, loop, and multiframe metadata exactly.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0128_loader_builtin_apng_animation_metadata_numeric" || {
    echo "not ok 1 - APNG animation metadata"
    exit 0
}

echo "ok 1 - APNG animation metadata is exact"
exit 0
