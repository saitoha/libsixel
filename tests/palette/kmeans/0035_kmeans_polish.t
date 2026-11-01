#!/bin/sh
# Ordinary K-means Ward polishing retains the signed mean.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_polish" || {
    echo "not ok 1 - Ordinary K-means Ward polishing retains the signed mean"
    exit 0
}
echo "ok 1 - Ordinary K-means Ward polishing retains the signed mean"
exit 0
