#!/bin/sh
# TAP wrapper validating builtin HDR gamma-target CMS conversion numerically.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=gamma" \
    --env "SIXEL_LOADER_PREFER_8BIT=0" \
    --env "SIXEL_TEST_HDR_NUMERIC_GAMMA=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (gamma numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (gamma numeric)"
exit 0
