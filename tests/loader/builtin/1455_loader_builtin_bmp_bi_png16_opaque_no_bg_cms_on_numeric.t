#!/bin/sh
# TAP wrapper for builtin BMP BI_PNG16 opaque no-bg CMS-on numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_BI_PNG16_OPAQUE_NO_BG_CMS_ON=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp bi-png16 opaque no-bg cms on numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp bi-png16 opaque no-bg cms on numeric)"
exit 0
