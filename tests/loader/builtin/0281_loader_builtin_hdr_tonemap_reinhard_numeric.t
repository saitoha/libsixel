#!/bin/sh
# TAP wrapper validating builtin HDR Reinhard tonemap numerically.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_HDR_EXPOSURE_EV=0" \
    --env "SIXEL_LOADER_HDR_TONEMAP=reinhard" \
    --env "SIXEL_TEST_HDR_NUMERIC_TONEMAP_REINHARD=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (hdr tonemap numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (hdr tonemap numeric)"
exit 0
