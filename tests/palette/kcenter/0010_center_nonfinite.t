#!/bin/sh
# Invalid float coordinates fail without a lossy retry.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_nonfinite" || {
    echo "not ok 1 - Invalid float coordinates fail without a lossy retry"
    exit 0
}
echo "ok 1 - Invalid float coordinates fail without a lossy retry"
exit 0
