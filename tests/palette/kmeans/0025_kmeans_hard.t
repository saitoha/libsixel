#!/bin/sh
# Ordinary K-means hard bins distinguish extended samples.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_hard" || {
    echo "not ok 1 - Ordinary K-means hard bins distinguish extended samples"
    exit 0
}
echo "ok 1 - Ordinary K-means hard bins distinguish extended samples"
exit 0
