#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify legacy RLE cannot repeat beyond the raster.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0134_loader_builtin_hdr_legacy_repeat_overrun_reject" || {
    printf '\n%s\n' "not ok 1 - HDR legacy RLE rejects a raster overrun"
    exit 0
}

printf '\n%s\n' "ok 1 - HDR legacy RLE rejects a raster overrun"
exit 0
