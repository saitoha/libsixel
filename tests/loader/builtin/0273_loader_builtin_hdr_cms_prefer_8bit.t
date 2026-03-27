#!/bin/sh
# TAP wrapper validating builtin HDR pixelformat checks with 8-bit CMS target.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=gamma" \
    --env "SIXEL_LOADER_PREFER_8BIT=1" \
    --env "SIXEL_TEST_EXPECT_HDR_CMS_PIXELFORMAT=RGB888" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (prefer 8bit)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (prefer 8bit)"
exit 0
