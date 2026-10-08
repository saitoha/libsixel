#!/bin/sh
# Heckbert nonfinite input fails without retry.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_nonfinite" || {
    echo "not ok 1 - Heckbert nonfinite input fails without retry"
    exit 0
}
echo "ok 1 - Heckbert nonfinite input fails without retry"
exit 0
