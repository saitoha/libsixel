#!/bin/sh
# Heckbert order sse numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/order_sse" || {
    echo "not ok 1 - Heckbert order_sse"
    exit 0
}
echo "ok 1 - Heckbert order_sse"
exit 0
