#!/bin/sh
# Verify builtin indexed TGA RGBA keeps PAL8 and collapses transparent indices
# into one transparent key index.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_TGA_NUMERIC_PAL_RGBA_TRANSPARENT_INDEX=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (tga palette rgba transparent index numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (tga palette rgba transparent index numeric)"
exit 0
