#!/bin/sh
# Heckbert order sse numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/order_sse" || {
    printf '\n%s\n' "not ok 1 - Heckbert order_sse"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert order_sse"
exit 0
