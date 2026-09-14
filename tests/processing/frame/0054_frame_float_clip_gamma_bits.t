#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame clip gamma bits.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state clip 0 || {
    echo "not ok 1 - float geometry clip_gamma_bits"
    exit 0
}
echo "ok 1 - float geometry clip_gamma_bits"
exit 0
