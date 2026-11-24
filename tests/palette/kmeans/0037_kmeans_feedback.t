#!/bin/sh
# K-means residual feedback distinguishes extended samples.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_feedback" || {
    printf '\n%s\n' "not ok 1 - K-means residual feedback distinguishes extended samples"
    exit 0
}
printf '\n%s\n' "ok 1 - K-means residual feedback distinguishes extended samples"
exit 0
