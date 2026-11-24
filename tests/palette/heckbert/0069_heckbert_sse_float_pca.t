#!/bin/sh
# Heckbert sse float pca numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/sse_float_pca" || {
    printf '\n%s\n' "not ok 1 - Heckbert sse_float_pca"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert sse_float_pca"
exit 0
