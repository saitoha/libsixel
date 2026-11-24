#!/bin/sh
# Float K-means rejects nonfinite samples without retry.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_nonfinite" || {
    printf '\n%s\n' "not ok 1 - Float K-means rejects nonfinite samples without retry"
    exit 0
}
printf '\n%s\n' "ok 1 - Float K-means rejects nonfinite samples without retry"
exit 0
