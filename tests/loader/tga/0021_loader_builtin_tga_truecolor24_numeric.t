#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify type-2 TGA BGR bytes expand to exact RGB component lanes.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0100_loader_builtin_tga_truecolor24_numeric" || {
    printf '\n%s\n' "not ok 1 - TGA type-2 exact RGB component lanes"
    exit 0
}

printf '\n%s\n' "ok 1 - TGA type-2 exact RGB component lanes"
exit 0
