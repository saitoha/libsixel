#!/bin/sh
# Heckbert axis din99d numerical contract.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/heckbert_policy/axis_din99d" || {
    echo "not ok 1 - Heckbert axis_din99d"
    exit 0
}
echo "ok 1 - Heckbert axis_din99d"
exit 0
