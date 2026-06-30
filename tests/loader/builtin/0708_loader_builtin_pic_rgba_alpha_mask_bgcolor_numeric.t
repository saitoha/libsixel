#!/bin/sh
# Verify builtin PIC RGBA keeps alpha-zero mask and blends semi-alpha with
# explicit background color.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TRANSPARENT_POLICY=composite" \
    --env "SIXEL_TEST_PIC_NUMERIC_RGBA_ALPHA_MASK_BGCOLOR=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (pic rgba alpha mask bgcolor numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (pic rgba alpha mask bgcolor numeric)"
exit 0
