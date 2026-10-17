#!/bin/sh
# Lloyd rebuilds representative statistics when a color changes membership.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/heckbert_policy/reassignment || {
    echo "not ok 1 - Lloyd final membership statistics"
    exit 0
}
echo "ok 1 - Lloyd final membership statistics"
exit 0
