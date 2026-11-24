#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA 16-bit palette index with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0083_loader_builtin_tga_index16_numeric" || {
    printf '\n%s\n' "not ok 1 - TGA 16-bit palette index"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA 16-bit palette index"
exit 0
