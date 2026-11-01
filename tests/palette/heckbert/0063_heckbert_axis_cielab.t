#!/bin/sh
# Heckbert axis cielab numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/axis_cielab" || {
    echo "not ok 1 - Heckbert axis_cielab"
    exit 0
}
echo "ok 1 - Heckbert axis_cielab"
exit 0
