#!/bin/sh
# Heckbert lloyd center numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/lloyd_center" || {
    echo "not ok 1 - Heckbert lloyd_center"
    exit 0
}
echo "ok 1 - Heckbert lloyd_center"
exit 0
