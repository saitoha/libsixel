#!/bin/sh
# Heckbert once snap runs only on the final palette.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_once" || {
    echo "not ok 1 - Heckbert once snap runs only on the final palette"
    exit 0
}
echo "ok 1 - Heckbert once snap runs only on the final palette"
exit 0
