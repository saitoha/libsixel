#!/bin/sh
# Run the focused builtin RGBA alpha-mask representation test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0063_loader_builtin_rgba_alpha_mask" || {
    echo "not ok 1 - builtin RGBA alpha-mask representation"
    exit 0
}

echo "ok 1 - builtin RGBA alpha-mask representation"
exit 0
