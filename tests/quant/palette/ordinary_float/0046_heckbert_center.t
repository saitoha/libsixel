#!/bin/sh
# Heckbert box center retains float extrema.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_center" || {
    echo "not ok 1 - Heckbert box center retains float extrema"
    exit 0
}
echo "ok 1 - Heckbert box center retains float extrema"
exit 0
