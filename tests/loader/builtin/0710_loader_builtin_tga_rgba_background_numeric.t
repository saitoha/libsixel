#!/bin/sh
# Verify builtin TGA RGBA preserves alpha without a background and composites
# all alpha against an explicit background.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_ALPHA_POLICY=composite" \
    --env "SIXEL_TEST_TGA_NUMERIC_RGBA_BACKGROUND=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - builtin TGA RGBA background handling"
    exit 0
}

echo "ok 1 - builtin TGA RGBA background handling"
exit 0
