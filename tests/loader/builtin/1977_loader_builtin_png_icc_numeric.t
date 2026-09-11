#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Fix builtin-CMS output type and representative samples for a profiled PNG.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0109_loader_builtin_png_icc_numeric" || {
    echo "not ok 1 - PNG embedded ICC numeric samples"
    exit 0
}

echo "ok 1 - PNG embedded ICC numeric samples"
exit 0
