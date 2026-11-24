#!/bin/sh
# K-means feedback retains requested partial snap stages.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_feedback_snap" || {
    printf '\n%s\n' "not ok 1 - K-means feedback retains requested partial snap stages"
    exit 0
}
printf '\n%s\n' "ok 1 - K-means feedback retains requested partial snap stages"
exit 0
