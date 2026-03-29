#!/bin/sh
# Verify builtin TGA truecolor RGBA keeps alpha-zero mask and composites RGB
# with default black / explicit background color.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_TGA_NUMERIC_RGBA_ALPHA_MASK_BGCOLOR=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (tga rgba alpha mask bgcolor numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (tga rgba alpha mask bgcolor numeric)"
exit 0
