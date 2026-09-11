#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF truncated raster sub-block rejection with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0076_loader_builtin_gif_truncated_raster_reject" || {
    echo "not ok 1 - GIF truncated raster sub-block rejection"
    exit 0
}

echo "ok 1 - GIF truncated raster sub-block rejection"
exit 0
