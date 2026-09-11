#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify PNG filters 0-4 reconstruct exact RGB.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0135_loader_builtin_png_filter_modes_numeric" 1>&2 || {
    echo "not ok" 1 - "PNG filters 0-4 reconstruct exact RGB"
    exit 0
}

echo "ok" 1 - "PNG filters 0-4 reconstruct exact RGB"

exit 0
