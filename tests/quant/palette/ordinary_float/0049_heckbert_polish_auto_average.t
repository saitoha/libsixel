#!/bin/sh
# Heckbert AUTO exports signed distinct-color means after Ward refinement.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_polish_auto_average" || {
    echo "not ok 1 - Heckbert Ward refinement retains signed means"
    exit 0
}
echo "ok 1 - Heckbert Ward refinement retains signed means"
exit 0
