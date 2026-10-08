#!/bin/sh
# Float allocation failure cannot retry byte or another solver.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_fault" || {
    echo "not ok 1 - Float allocation failure cannot retry byte or another solver"
    exit 0
}
echo "ok 1 - Float allocation failure cannot retry byte or another solver"
exit 0
