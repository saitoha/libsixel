#!/bin/sh
# Heckbert distinguishes subbyte float colors.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_unique" || {
    echo "not ok 1 - Heckbert distinguishes subbyte float colors"
    exit 0
}
echo "ok 1 - Heckbert distinguishes subbyte float colors"
exit 0
