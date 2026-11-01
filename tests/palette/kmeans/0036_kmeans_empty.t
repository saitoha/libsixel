#!/bin/sh
# K-means empty-cluster repair retains signed donor moments.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_empty" || {
    echo "not ok 1 - K-means empty-cluster repair retains signed donor moments"
    exit 0
}
echo "ok 1 - K-means empty-cluster repair retains signed donor moments"
exit 0
