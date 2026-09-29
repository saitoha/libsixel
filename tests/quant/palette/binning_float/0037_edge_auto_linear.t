#!/bin/sh
# Verify edge auto linear.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/binning_float/0001_binning_contract \
    edge 1 3 0 || {
    echo "not ok 1 - edge auto linear"
    exit 0
}
echo "ok 1 - edge auto linear"
exit 0
