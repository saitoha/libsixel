#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA out-of-range index compatibility fallback with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0087_loader_builtin_tga_oob_index_fallback_numeric" || {
    printf '\n%s\n' "not ok 1 - TGA out-of-range index compatibility fallback"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA out-of-range index compatibility fallback"
exit 0
