#!/bin/sh
# Reduced ordinary palettes select extended source candidates.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/center_reduce" || {
    echo "not ok 1 - Reduced ordinary palettes select extended source candidates"
    exit 0
}
echo "ok 1 - Reduced ordinary palettes select extended source candidates"
exit 0
