#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Policy: docs/loader/builtin-cms.md
# Verify PNG ICC allocation fallback leaves no live allocation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0169_loader_builtin_png_icc_allocation_failures" 1>&2 || {
    echo "not ok" 1 - "PNG ICC allocation failure cleanup"
    exit 0
}
echo "ok" 1 - "PNG ICC allocation failure cleanup"
exit 0
