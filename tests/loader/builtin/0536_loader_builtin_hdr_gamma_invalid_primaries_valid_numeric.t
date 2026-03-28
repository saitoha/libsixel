#!/bin/sh
set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear" \
    --env "SIXEL_LOADER_PREFER_8BIT=0" \
    --env "SIXEL_TEST_HDR_NUMERIC_GAMMA_INVALID_PRIMARIES_VALID=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (hdr gamma invalid primaries valid numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (hdr gamma invalid primaries valid numeric)"
exit 0
