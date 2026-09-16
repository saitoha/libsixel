#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify CgBI premultiplied BGRA becomes exact straight RGB.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0157_loader_builtin_png_cgbi_numeric" 1>&2 || {
    echo "not ok" 1 - "builtin CgBI normalization produces exact RGB"
    exit 0
}

echo "ok" 1 - "builtin CgBI normalization produces exact RGB"
exit 0
