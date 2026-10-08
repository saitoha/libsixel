#!/bin/sh
# Heckbert rebuilds after a float allocation failure.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/heckbert_reuse" || {
    echo "not ok 1 - Heckbert rebuilds after a float allocation failure"
    exit 0
}
echo "ok 1 - Heckbert rebuilds after a float allocation failure"
exit 0
