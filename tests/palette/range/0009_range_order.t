#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Byte bin refinement is independent of sample traversal order.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0009_range_order" || {
    echo "not ok 1 - 0009_range_order"
    exit 0
}
echo "ok 1 - 0009_range_order"
exit 0
