#!/bin/sh
# Verify builtin PIC RGBA composites all alpha against an explicit background.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_ALPHA_POLICY=composite" \
    --env "SIXEL_TEST_PIC_NUMERIC_RGBA_BACKGROUND=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - builtin PIC RGBA background composition"
    exit 0
}

echo "ok 1 - builtin PIC RGBA background composition"
exit 0
