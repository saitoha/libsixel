#!/bin/sh
# Ordinary K-means Ward retains fractional soft-bin populations.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_fractional" || {
    printf '\n%s\n' "not ok 1 - Ordinary K-means Ward retains fractional soft-bin populations"
    exit 0
}
printf '\n%s\n' "ok 1 - Ordinary K-means Ward retains fractional soft-bin populations"
exit 0
