#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify an RLE packet cannot run past the declared raster.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0123_loader_builtin_tga_rle_overrun_reject" || {
    printf '\n%s\n' "not ok 1 - TGA RLE packet overrun is rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA RLE packet overrun is rejected"
exit 0
