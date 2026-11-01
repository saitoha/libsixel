#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Release the failed PAL8 attempt and preserve RGB index fallback.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0239_tga_origin_pal_fallback_numeric" 1>&2 || {
    echo "not ok 1 - Release the failed PAL8 attempt and preserve RGB index fallback"
    exit 0
}
echo "ok 1 - Release the failed PAL8 attempt and preserve RGB index fallback"
exit 0
