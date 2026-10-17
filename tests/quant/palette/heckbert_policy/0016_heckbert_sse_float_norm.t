#!/bin/sh
# Heckbert sse float norm numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/sse_float_norm" || {
    echo "not ok 1 - Heckbert sse_float_norm"
    exit 0
}
echo "ok 1 - Heckbert sse_float_norm"
exit 0
