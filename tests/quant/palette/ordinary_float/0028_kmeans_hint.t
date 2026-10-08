#!/bin/sh
# Typed K-means input overrides a byte preference hint.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_hint" || {
    echo "not ok 1 - Typed K-means input overrides a byte preference hint"
    exit 0
}
echo "ok 1 - Typed K-means input overrides a byte preference hint"
exit 0
