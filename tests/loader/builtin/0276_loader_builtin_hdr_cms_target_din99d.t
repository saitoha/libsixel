#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# TAP wrapper validating builtin HDR pixelformat checks with DIN99d CMS target.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BUILTIN_PIXELFORMAT_BASELINE=1" \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=din99d" \
    --env "SIXEL_LOADER_PREFER_8BIT=0" \
    --env "SIXEL_TEST_EXPECT_HDR_CMS_PIXELFORMAT=DIN99DFLOAT32" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (din99d target)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (din99d target)"
exit 0
