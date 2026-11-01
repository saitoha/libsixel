#!/bin/sh
# Failed float K-means allocation is not retried as byte.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_fault" || {
    echo "not ok 1 - Failed float K-means allocation is not retried as byte"
    exit 0
}
echo "ok 1 - Failed float K-means allocation is not retried as byte"
exit 0
