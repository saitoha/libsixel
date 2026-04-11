#!/bin/sh
# TAP wrapper for builtin BMP V5 linked-profile ignored numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_V5_LINKED_PROFILE_IGNORED=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp v5 linked-profile ignored numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp v5 linked-profile ignored numeric)"
exit 0
