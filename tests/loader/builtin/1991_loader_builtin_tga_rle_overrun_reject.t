#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify an RLE packet cannot run past the declared raster.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0123_loader_builtin_tga_rle_overrun_reject" || {
    echo "not ok 1 - TGA RLE packet overrun is rejected"
    exit 0
}

echo "ok 1 - TGA RLE packet overrun is rejected"
exit 0
