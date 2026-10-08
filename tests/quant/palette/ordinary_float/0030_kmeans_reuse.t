#!/bin/sh
# K-means can rebuild after float allocation failure.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_reuse" || {
    echo "not ok 1 - K-means can rebuild after float allocation failure"
    exit 0
}
echo "ok 1 - K-means can rebuild after float allocation failure"
exit 0
