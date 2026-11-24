#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify direct-color raster truncation is rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0122_loader_builtin_tga_truncated_direct_reject" || {
    printf '\n%s\n' "not ok 1 - TGA truncated direct raster is rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA truncated direct raster is rejected"
exit 0
