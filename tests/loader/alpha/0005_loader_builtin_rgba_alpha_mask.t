#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Run the focused builtin RGBA alpha-mask representation test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0063_loader_builtin_rgba_alpha_mask" || {
    printf '\n%s\n' "not ok 1 - builtin RGBA alpha-mask representation"
    exit 0
}

printf '\n%s\n' "ok 1 - builtin RGBA alpha-mask representation"
exit 0
