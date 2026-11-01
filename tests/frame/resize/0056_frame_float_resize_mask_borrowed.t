#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify extended color and bounded transparent-mask interpolation together.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state resize 9 || {
    echo "not ok 1 - float resize with mask"
    exit 0
}
echo "ok 1 - float resize with mask"
exit 0
