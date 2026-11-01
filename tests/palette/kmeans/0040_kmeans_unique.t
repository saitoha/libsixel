#!/bin/sh
# K-means Ward counts subbyte float colors independently.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_unique" || {
    echo "not ok 1 - K-means Ward counts subbyte float colors independently"
    exit 0
}
echo "ok 1 - K-means Ward counts subbyte float colors independently"
exit 0
