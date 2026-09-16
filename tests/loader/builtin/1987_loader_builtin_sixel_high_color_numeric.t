#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/sixel.md
# Isolate final-palette collapse after a color-register redefinition.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0119_loader_builtin_sixel_high_color_numeric" || {
    echo "not ok 1 - SIXEL high-color adapter semantics"
    exit 0
}

echo "ok 1 - SIXEL high-color adapter semantics"
exit 0
