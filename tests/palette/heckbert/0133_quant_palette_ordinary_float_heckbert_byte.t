#!/bin/sh
# Byte Heckbert preserves its bounded histogram.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_byte" || {
    echo "not ok 1 - Byte Heckbert preserves its bounded histogram"
    exit 0
}
echo "ok 1 - Byte Heckbert preserves its bounded histogram"
exit 0
