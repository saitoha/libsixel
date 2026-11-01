#!/bin/sh
# Byte K-means keeps byte-only palette storage.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_byte" || {
    echo "not ok 1 - Byte K-means keeps byte-only palette storage"
    exit 0
}
echo "ok 1 - Byte K-means keeps byte-only palette storage"
exit 0
