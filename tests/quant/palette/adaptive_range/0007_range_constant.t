#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Constant black and a white highlight do not divide by zero.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/adaptive_range/0007_range_constant" || {
    echo "not ok 1 - 0007_range_constant"
    exit 0
}
echo "ok 1 - 0007_range_constant"
exit 0
