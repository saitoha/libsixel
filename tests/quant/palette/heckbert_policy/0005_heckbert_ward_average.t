#!/bin/sh
# Heckbert ward average numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/ward_average" || {
    echo "not ok 1 - Heckbert ward_average"
    exit 0
}
echo "ok 1 - Heckbert ward_average"
exit 0
