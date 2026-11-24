#!/bin/sh
# Heckbert box center retains float extrema.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_center" || {
    printf '\n%s\n' "not ok 1 - Heckbert box center retains float extrema"
    exit 0
}
printf '\n%s\n' "ok 1 - Heckbert box center retains float extrema"
exit 0
