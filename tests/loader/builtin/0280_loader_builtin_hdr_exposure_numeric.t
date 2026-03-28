#!/bin/sh
# TAP wrapper validating builtin HDR exposure application numerically.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_HDR_EXPOSURE_EV=1" \
    --env "SIXEL_LOADER_HDR_TONEMAP=none" \
    --env "SIXEL_TEST_HDR_NUMERIC_EXPOSURE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (hdr exposure numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (hdr exposure numeric)"
exit 0
