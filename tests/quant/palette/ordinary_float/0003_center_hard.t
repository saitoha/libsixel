#!/bin/sh
# K-center hard bins expand addresses without clipping source coordinates.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_hard" || {
    echo "not ok 1 - K-center hard bins expand addresses without clipping source coordinates"
    exit 0
}
echo "ok 1 - K-center hard bins expand addresses without clipping source coordinates"
exit 0
