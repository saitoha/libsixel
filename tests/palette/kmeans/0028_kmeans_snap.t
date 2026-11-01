#!/bin/sh
# Ordinary K-means honors partial iteration and final snap.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_snap" || {
    echo "not ok 1 - Ordinary K-means honors partial iteration and final snap"
    exit 0
}
echo "ok 1 - Ordinary K-means honors partial iteration and final snap"
exit 0
