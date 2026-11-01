#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify direct-color raster truncation is rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0122_loader_builtin_tga_truncated_direct_reject" || {
    echo "not ok 1 - TGA truncated direct raster is rejected"
    exit 0
}

echo "ok 1 - TGA truncated direct raster is rejected"
exit 0
