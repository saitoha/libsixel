#!/bin/sh
# Heckbert Ward refinement retains signed means.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_polish" || {
    echo "not ok 1 - Heckbert Ward refinement retains signed means"
    exit 0
}
echo "ok 1 - Heckbert Ward refinement retains signed means"
exit 0
