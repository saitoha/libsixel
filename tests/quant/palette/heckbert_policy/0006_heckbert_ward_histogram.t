#!/bin/sh
# Heckbert ward histogram numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/ward_histogram" || {
    echo "not ok 1 - Heckbert ward_histogram"
    exit 0
}
echo "ok 1 - Heckbert ward_histogram"
exit 0
