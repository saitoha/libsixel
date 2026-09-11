#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify PNG Adam7 indexed4 reconstructs exact RGB.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0136_loader_builtin_png_adam7_indexed4_digest" 1>&2 || {
    echo "not ok" 1 - "PNG Adam7 indexed4 reconstructs exact RGB"
    exit 0
}

echo "ok" 1 - "PNG Adam7 indexed4 reconstructs exact RGB"

exit 0
