#!/bin/sh
# K-means residual feedback distinguishes extended samples.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_feedback" || {
    echo "not ok 1 - K-means residual feedback distinguishes extended samples"
    exit 0
}
echo "ok 1 - K-means residual feedback distinguishes extended samples"
exit 0
