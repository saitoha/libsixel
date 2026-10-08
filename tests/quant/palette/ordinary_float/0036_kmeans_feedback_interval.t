#!/bin/sh
# K-means feedback respects its iteration interval.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_feedback_interval" || {
    echo "not ok 1 - K-means feedback respects its iteration interval"
    exit 0
}
echo "ok 1 - K-means feedback respects its iteration interval"
exit 0
