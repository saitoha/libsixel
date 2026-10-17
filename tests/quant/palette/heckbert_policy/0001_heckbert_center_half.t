#!/bin/sh
# Heckbert center half numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/center_half" || {
    echo "not ok 1 - Heckbert center_half"
    exit 0
}
echo "ok 1 - Heckbert center_half"
exit 0
