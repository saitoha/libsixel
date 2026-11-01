#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA 16-bit truecolor expansion with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0080_loader_builtin_tga_truecolor16_numeric" || {
    echo "not ok 1 - TGA 16-bit truecolor expansion"
    exit 0
}

echo "ok 1 - TGA 16-bit truecolor expansion"
exit 0
