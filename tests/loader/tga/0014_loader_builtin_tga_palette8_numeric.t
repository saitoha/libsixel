#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA 8-bit grayscale palette entry with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0081_loader_builtin_tga_palette8_numeric" || {
    printf '\n%s\n' "not ok 1 - TGA 8-bit grayscale palette entry"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA 8-bit grayscale palette entry"
exit 0
