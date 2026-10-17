#!/bin/sh
# Heckbert axis rgb numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/axis_rgb" || {
    echo "not ok 1 - Heckbert axis_rgb"
    exit 0
}
echo "ok 1 - Heckbert axis_rgb"
exit 0
