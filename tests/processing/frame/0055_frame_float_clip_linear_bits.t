#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame clip linear bits.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state clip 1 || {
    echo "not ok 1 - float geometry clip_linear_bits"
    exit 0
}
echo "ok 1 - float geometry clip_linear_bits"
exit 0
