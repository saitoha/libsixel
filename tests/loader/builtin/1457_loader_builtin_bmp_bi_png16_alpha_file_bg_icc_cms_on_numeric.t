#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin BMP BI_PNG16 uses its PNG background with ICC and CMS on.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_ALPHA_POLICY=composite" \
    --env "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_ALPHA_FILE_BG_ICC_CMS_ON=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - builtin BMP BI_PNG16 file background with ICC"
    exit 0
}

echo "ok 1 - builtin BMP BI_PNG16 file background with ICC"
exit 0
