#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify type-3 TGA gray samples expand to exact RGB bytes.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0101_loader_builtin_tga_grayscale8_numeric" || {
    printf '\n%s\n' "not ok 1 - TGA type-3 exact grayscale expansion"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA type-3 exact grayscale expansion"
exit 0
