#!/bin/sh
# TAP wrapper for builtin BMP V2 BI_ALPHABITFIELDS failure checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_V2_ALPHABITFIELDS=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail v2 alphabitfields numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail v2 alphabitfields numeric)"
exit 0
