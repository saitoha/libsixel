#!/bin/sh
# Ordinary K-means preserves extended coordinates in all float spaces.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_sources" || {
    echo "not ok 1 - Ordinary K-means preserves extended coordinates in all float spaces"
    exit 0
}
echo "ok 1 - Ordinary K-means preserves extended coordinates in all float spaces"
exit 0
