#!/bin/sh
# Heckbert average half numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/average_half" || {
    echo "not ok 1 - Heckbert average_half"
    exit 0
}
echo "ok 1 - Heckbert average_half"
exit 0
