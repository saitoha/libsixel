#!/bin/sh
# Heckbert occurrence mean retains float values.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_histogram" || {
    echo "not ok 1 - Heckbert occurrence mean retains float values"
    exit 0
}
echo "ok 1 - Heckbert occurrence mean retains float values"
exit 0
